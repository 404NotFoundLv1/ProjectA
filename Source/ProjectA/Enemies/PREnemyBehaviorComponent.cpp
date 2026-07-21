#include "Enemies/PREnemyBehaviorComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRSpawnManager.h"
#include "Enemies/PREnemyCharacter.h"
#include "Enemies/PREnemyAIController.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "Enemies/PREnemyThreatComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UPREnemyBehaviorComponent::UPREnemyBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPREnemyBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBehavior();
	Super::EndPlay(EndPlayReason);
}

void UPREnemyBehaviorComponent::StartBehavior()
{
	APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetOwner());
	if (!Enemy || !Enemy->HasAuthority() || !Enemy->GetEnemyDefinition())
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(EvaluationTimer, this, &UPREnemyBehaviorComponent::EvaluateBehavior, 0.20f, true, 0.20f);
}

void UPREnemyBehaviorComponent::StopBehavior()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
		World->GetTimerManager().ClearTimer(TelegraphTimer);
	}
	PendingActionIndex = INDEX_NONE;
	bTelegraphPending = false;
}

int32 UPREnemyBehaviorComponent::SelectUsableAction(APREnemyCharacter* Enemy, APRCharacter* Target, const float CurrentTime) const
{
	const UPREnemyDefinitionDataAsset* Definition = Enemy ? Enemy->GetEnemyDefinition() : nullptr;
	if (!Definition || !Target)
	{
		return INDEX_NONE;
	}
	const float Distance = FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
	for (int32 Index = 0; Index < Definition->Actions.Num(); ++Index)
	{
		const FPREnemyActionDefinition& Action = Definition->Actions[Index];
		if (Distance >= Action.MinimumRange && Distance <= Action.MaximumRange && CurrentTime >= NextActionTimes.FindRef(Action.ActionTag))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UPREnemyBehaviorComponent::EvaluateBehavior()
{
	APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetOwner());
	if (!Enemy || Enemy->IsDead() || !Enemy->GetEnemyDefinition() || bTelegraphPending)
	{
		return;
	}
	UPRAbilitySystemComponent* EnemyASC = Enemy->GetProjectRiftAbilitySystemComponent();
	if (!EnemyASC || EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned)
		|| EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered))
	{
		return;
	}
	if (UPREnemyThreatComponent* Threat = Enemy->GetEnemyThreatComponent())
	{
		Threat->PruneInvalidThreat();
		CurrentTarget = Threat->SelectTarget(Enemy->GetEnemyDefinition()->TargetPolicy);
	}
	APRCharacter* Target = CurrentTarget.Get();
	if (!Target)
	{
		return;
	}
	const int32 ActionIndex = SelectUsableAction(Enemy, Target, GetWorld()->GetTimeSeconds());
	if (ActionIndex == INDEX_NONE)
	{
		if (APREnemyAIController* Controller = Cast<APREnemyAIController>(Enemy->GetController()))
		{
			Controller->RequestMoveToTarget(Target, 140.0f);
		}
		return;
	}
	const FPREnemyActionDefinition& Action = Enemy->GetEnemyDefinition()->Actions[ActionIndex];
	if (APREnemyAIController* Controller = Cast<APREnemyAIController>(Enemy->GetController()))
	{
		Controller->StopForEnemyAction();
	}
	PendingTarget = Target;
	PendingActionIndex = ActionIndex;
	bTelegraphPending = true;
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement()) Movement->StopMovementImmediately();
	GetWorld()->GetTimerManager().SetTimer(TelegraphTimer, this, &UPREnemyBehaviorComponent::ExecutePendingAction,
		FMath::Max(0.0f, Action.TelegraphSeconds), false);
}

void UPREnemyBehaviorComponent::ExecutePendingAction()
{
	APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetOwner());
	APRCharacter* Target = PendingTarget.Get();
	const UPREnemyDefinitionDataAsset* Definition = Enemy ? Enemy->GetEnemyDefinition() : nullptr;
	const int32 ActionIndex = PendingActionIndex;
	bTelegraphPending = false;
	PendingActionIndex = INDEX_NONE;
	if (!Enemy || !Target || Enemy->IsDead() || !Definition || !Definition->Actions.IsValidIndex(ActionIndex)) return;
	const FPREnemyActionDefinition& Action = Definition->Actions[ActionIndex];
	if (UPRAbilitySystemComponent* ASC = Enemy->GetProjectRiftAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned)
			|| (ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered)
				&& Action.InterruptPolicy == EPREnemyActionInterruptPolicy::AnyStaggerOrStun))
		{
			return;
		}
	}
	if (ExecuteAction(Enemy, Target, Action))
	{
		NextActionTimes.Add(Action.ActionTag, GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, Action.CooldownSeconds));
	}
}

void UPREnemyBehaviorComponent::ApplyActionToTarget(APREnemyCharacter* Enemy, APRCharacter* Target, const FPREnemyActionDefinition& Action, const int32 Repetitions)
{
	if (!Enemy || !Target || !Target->IsAlive()) return;
	UPRAbilitySystemComponent* Source = Enemy->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* TargetASC = Target->GetProjectRiftAbilitySystemComponent();
	if (!Source || !TargetASC) return;
	for (int32 Index = 0; Index < FMath::Max(1, Repetitions); ++Index)
	{
		if (Action.BaseDamage > 0.0f)
		{
			FPRDamageRequest Request;
			Request.BaseDamage = Action.BaseDamage;
			Request.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
			Request.HitReaction = Action.HitReaction;
			Request.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
			UPRCombatEffectLibrary::ApplyDamageRequestToTarget(Source, TargetASC, Request, Enemy);
		}
	}
	if (Action.StatusEffect.EffectClass)
	{
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(Source, TargetASC, Action.StatusEffect, Enemy);
	}
}

bool UPREnemyBehaviorComponent::HasProjectileLineOfSight(const APREnemyCharacter* Enemy, const APRCharacter* Target) const
{
	UWorld* World = GetWorld();
	if (!World || !Enemy || !Target)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectRiftEnemyProjectile), false, Enemy);
	QueryParams.AddIgnoredActor(Enemy);
	FHitResult Hit;
	const FVector Start = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 72.0f);
	const FVector End = Target->GetActorLocation() + FVector(0.0f, 0.0f, 52.0f);
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams)
		&& Hit.GetActor() == Target;
}

bool UPREnemyBehaviorComponent::RepositionForMobilityAction(APREnemyCharacter* Enemy, const APRCharacter* Target, const FPREnemyActionDefinition& Action) const
{
	UWorld* World = GetWorld();
	if (!World || !Enemy || !Target)
	{
		return false;
	}

	const FVector EnemyLocation = Enemy->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector AwayFromTarget = (EnemyLocation - TargetLocation).GetSafeNormal2D();
	if (AwayFromTarget.IsNearlyZero())
	{
		AwayFromTarget = -Target->GetActorForwardVector().GetSafeNormal2D();
	}
	const float DesiredRange = Action.Kind == EPREnemyActionKind::Burrow ? 170.0f : 120.0f;
	const FVector DesiredLocation = TargetLocation + AwayFromTarget * DesiredRange;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation NavigationLocation;
	if (!Navigation || !Navigation->ProjectPointToNavigation(DesiredLocation, NavigationLocation, FVector(200.0f, 200.0f, 300.0f)))
	{
		return false;
	}

	// Burrow deliberately bypasses a direct line of sight; pounce/dash must have
	// a clear path so players can counter it with geometry.
	if (Action.Kind == EPREnemyActionKind::Pounce)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectRiftEnemyPounce), false, Enemy);
		QueryParams.AddIgnoredActor(Enemy);
		FHitResult Obstruction;
		if (World->LineTraceSingleByChannel(Obstruction, EnemyLocation + FVector(0.0f, 0.0f, 50.0f), NavigationLocation.Location + FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, QueryParams))
		{
			return false;
		}
	}
	return Enemy->SetActorLocation(NavigationLocation.Location, true);
}

bool UPREnemyBehaviorComponent::ExecuteAction(APREnemyCharacter* Enemy, APRCharacter* Target, const FPREnemyActionDefinition& Action)
{
	switch (Action.Kind)
	{
	case EPREnemyActionKind::Projectile:
		if (!HasProjectileLineOfSight(Enemy, Target))
		{
			return false;
		}
		ApplyActionToTarget(Enemy, Target, Action);
		return true;
	case EPREnemyActionKind::Burrow:
	case EPREnemyActionKind::Pounce:
		if (!RepositionForMobilityAction(Enemy, Target, Action))
		{
			return false;
		}
		ApplyActionToTarget(Enemy, Target, Action);
		return true;
	case EPREnemyActionKind::Burst:
		ApplyActionToTarget(Enemy, Target, Action, 3);
		return true;
	case EPREnemyActionKind::ShieldPulse:
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams Query(ECC_Pawn);
		GetWorld()->OverlapMultiByObjectType(Overlaps, Enemy->GetActorLocation(), FQuat::Identity, Query, FCollisionShape::MakeSphere(Action.Radius));
		const int32 MaxRecipients = Action.BaseDamage >= 30.0f ? 5 : 3;
		const float ShieldDuration = Action.BaseDamage >= 30.0f ? 7.0f : 5.0f;
		int32 Shielded = 0;
		for (const FOverlapResult& Hit : Overlaps)
		{
			APREnemyCharacter* Ally = Cast<APREnemyCharacter>(Hit.GetActor());
			if (Ally && !Ally->IsDead() && Shielded < MaxRecipients)
			{
				UPRCombatEffectLibrary::ApplyEnemyTemporaryShieldToTarget(Enemy->GetProjectRiftAbilitySystemComponent(), Ally->GetProjectRiftAbilitySystemComponent(), Action.BaseDamage, ShieldDuration, Enemy);
				++Shielded;
			}
		}
		return Shielded > 0;
	}
	case EPREnemyActionKind::Summon:
		return Action.SummonCount > 0 && Cast<APRSpawnManager>(Enemy->GetOwner())
			&& Cast<APRSpawnManager>(Enemy->GetOwner())->SpawnSummonedEnemies(Action.SummonDefinitionId, Action.SummonCount) > 0;
	case EPREnemyActionKind::Disrupt:
	{
		UPRAbilitySystemComponent* TargetASC = Target->GetProjectRiftAbilitySystemComponent();
		if (TargetASC && !TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Grace_Disruption)
			&& HasProjectileLineOfSight(Enemy, Target))
		{
			ApplyActionToTarget(Enemy, Target, Action);
			UPRCombatEffectLibrary::ApplyStatusEffectToTarget(Enemy->GetProjectRiftAbilitySystemComponent(), TargetASC,
				FPRTargetStatusEffectDefinition(UPRDisruptionGraceGameplayEffect::StaticClass(), 1.0f, 8.0f), Enemy);
			return true;
		}
		return false;
	}
	case EPREnemyActionKind::Exploder:
		if (const AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			for (const TObjectPtr<APlayerState>& CandidatePlayerState : GameState->PlayerArray)
			{
				APRCharacter* Candidate = CandidatePlayerState ? Cast<APRCharacter>(CandidatePlayerState->GetPawn()) : nullptr;
				if (Candidate && Candidate->IsAlive()
					&& FVector::DistSquared(Candidate->GetActorLocation(), Enemy->GetActorLocation()) <= FMath::Square(Action.Radius))
				{
					ApplyActionToTarget(Enemy, Candidate, Action);
				}
			}
		}
		Enemy->ApplyEnemyDamage(100000.0f, nullptr);
		return true;
	default:
		ApplyActionToTarget(Enemy, Target, Action);
		return true;
	}
}
