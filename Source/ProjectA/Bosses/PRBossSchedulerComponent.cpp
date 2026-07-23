#include "Bosses/PRBossSchedulerComponent.h"

#include "Bosses/PRBossCharacter.h"
#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Bosses/PRBossTelegraphActor.h"
#include "Characters/PRCharacter.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyThreatComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

UPRBossSchedulerComponent::UPRBossSchedulerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UPRBossSchedulerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopScheduler();
	Super::EndPlay(EndPlayReason);
}

bool UPRBossSchedulerComponent::StartScheduler(APRBossCharacter* InBoss, UPRBossDefinitionDataAsset* InDefinition, const int32 InFrozenPlayerCount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InBoss || !InDefinition)
	{
		return false;
	}

	FString Diagnostic;
	if (!InDefinition->ValidateDefinition(Diagnostic))
	{
		return false;
	}

	Boss = InBoss;
	Definition = InDefinition;
	FrozenPlayerCount = FMath::Clamp(InFrozenPlayerCount, 1, 4);
	ResetScheduler();
	GetWorld()->GetTimerManager().SetTimer(EvaluationTimer, this, &UPRBossSchedulerComponent::EvaluateScheduler, 0.2f, true);
	return true;
}

void UPRBossSchedulerComponent::StopScheduler()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
		World->GetTimerManager().ClearTimer(TelegraphTimer);
		World->GetTimerManager().ClearTimer(StaggerTimer);
		World->GetTimerManager().ClearTimer(ExecutionTimer);
		World->GetTimerManager().ClearTimer(PhaseTransitionTimer);
	}
	if (ActiveTelegraph.IsValid()) { ActiveTelegraph->Destroy(); }
	ActiveTelegraph.Reset();
	RuntimeSnapshot = FPRBossRuntimeSnapshot();
}

void UPRBossSchedulerComponent::ResetScheduler()
{
	NextPatternTimes.Reset();
	RecentPatterns.Reset();
	WeakPointDamage = 0.0f;
	CurrentTargets.Reset();
	LockedTargetLocations.Reset();
	DecisionOrdinal = 0;
	RuntimeSnapshot = FPRBossRuntimeSnapshot();
	if (const UPRBossDefinitionDataAsset* BossDefinition = Definition.Get())
	{
		RuntimeSnapshot.BossId = BossDefinition->BossId;
		RuntimeSnapshot.State = EPRBossRuntimeState::Selecting;
		RuntimeSnapshot.PhaseIndex = 0;
		RuntimeSnapshot.PhaseId = BossDefinition->Phases[0].PhaseId;
		RuntimeSnapshot.bVisible = true;
		if (APRBossCharacter* BossActor = Boss.Get())
		{
			BossActor->ApplyPhaseModifiers(BossDefinition->Phases[0]);
		}
	}
	PublishSnapshot();
}

void UPRBossSchedulerComponent::NotifyWeakPointDamage(const FName WeakPointId, const float AppliedDamage)
{
	const UPRBossDefinitionDataAsset* BossDefinition = Definition.Get();
	if (!BossDefinition || RuntimeSnapshot.State != EPRBossRuntimeState::Executing || !RuntimeSnapshot.bInterruptible || !FMath::IsFinite(AppliedDamage) || AppliedDamage <= 0.0f)
	{
		return;
	}

	const FPRBossWeakPointDefinition* WeakPoint = BossDefinition->WeakPoints.FindByPredicate([WeakPointId](const FPRBossWeakPointDefinition& Candidate) { return Candidate.WeakPointId == WeakPointId; });
	if (!WeakPoint)
	{
		return;
	}

	WeakPointDamage += AppliedDamage;
	if (WeakPoint->InterruptDamageThreshold > 0.0f && WeakPointDamage >= WeakPoint->InterruptDamageThreshold * BossDefinition->StaggerScaling.GetForPlayerCount(FrozenPlayerCount))
	{
		WeakPointDamage = 0.0f;
		GetWorld()->GetTimerManager().ClearTimer(TelegraphTimer);
		GetWorld()->GetTimerManager().ClearTimer(ExecutionTimer);
		if (ActiveTelegraph.IsValid()) { ActiveTelegraph->Destroy(); }
		ActiveTelegraph.Reset();
		RuntimeSnapshot.State = EPRBossRuntimeState::Staggered;
		RuntimeSnapshot.bStaggered = true;
		RuntimeSnapshot.bInterruptible = false;
		if (APRBossCharacter* BossActor = Boss.Get())
		{
			BossActor->GetAbilitySystemComponent()->ExecuteGameplayCue(ProjectRiftGameplayTags::GameplayCue_Boss_Staggered);
		}
		GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &UPRBossSchedulerComponent::ClearStagger, BossDefinition->StaggerDurationSeconds, false);
		PublishSnapshot();
	}
}

void UPRBossSchedulerComponent::EvaluateScheduler()
{
	if (!Boss.IsValid() || !Definition.IsValid())
	{
		return;
	}
	if (RuntimeSnapshot.State == EPRBossRuntimeState::Telegraphing)
	{
		RuntimeSnapshot.TelegraphRemainingSeconds = FMath::Max(0.0f, TelegraphEndTime - GetWorld()->GetTimeSeconds());
		PublishSnapshot();
		return;
	}
	if (RuntimeSnapshot.State != EPRBossRuntimeState::Selecting) { return; }
	RuntimeSnapshot.HealthPercent = Boss->GetHealthPercent();
	if (UpdatePhaseForHealthPercent())
	{
		return;
	}
	SelectNextPattern();
}

void UPRBossSchedulerComponent::SelectNextPattern()
{
	UPRBossDefinitionDataAsset* BossDefinition = Definition.Get();
	if (!BossDefinition || BossDefinition->AbilityPatterns.IsEmpty())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const FPRBossPhaseDefinition* Phase = BossDefinition->Phases.IsValidIndex(RuntimeSnapshot.PhaseIndex)
		? &BossDefinition->Phases[RuntimeSnapshot.PhaseIndex] : nullptr;
	TArray<const FPRBossAbilityPatternDefinition*> Eligible;
	for (const FPRBossAbilityPatternDefinition& Pattern : BossDefinition->AbilityPatterns)
	{
		const bool bEnabledInPhase = !Phase || Phase->EnabledPatternIds.IsEmpty() || Phase->EnabledPatternIds.Contains(Pattern.PatternId);
		if (bEnabledInPhase && Pattern.Weight > 0.0f && NextPatternTimes.FindRef(Pattern.PatternId) <= Now && !RecentPatterns.Contains(Pattern.PatternId))
		{
			Eligible.Add(&Pattern);
		}
	}
	if (Eligible.IsEmpty())
	{
		for (const FPRBossAbilityPatternDefinition& Pattern : BossDefinition->AbilityPatterns)
		{
			const bool bEnabledInPhase = !Phase || Phase->EnabledPatternIds.IsEmpty() || Phase->EnabledPatternIds.Contains(Pattern.PatternId);
			if (bEnabledInPhase && Pattern.Weight > 0.0f && NextPatternTimes.FindRef(Pattern.PatternId) <= Now)
			{
				Eligible.Add(&Pattern);
			}
		}
	}
	const FPRBossAbilityPatternDefinition* Candidate = nullptr;
	if (!Eligible.IsEmpty())
	{
		float TotalWeight = 0.0f;
		for (const FPRBossAbilityPatternDefinition* Pattern : Eligible) { TotalWeight += Pattern->Weight; }
		FRandomStream RandomStream(GetTypeHash(BossDefinition->BossId) ^ (DecisionOrdinal * 2654435761u));
		float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
		for (const FPRBossAbilityPatternDefinition* Pattern : Eligible)
		{
			Roll -= Pattern->Weight;
			if (Roll <= 0.0f) { Candidate = Pattern; break; }
		}
		Candidate = Candidate ? Candidate : Eligible.Last();
	}
	if (!Candidate)
	{
		return;
	}

	RuntimeSnapshot.State = EPRBossRuntimeState::Telegraphing;
	RuntimeSnapshot.ActivePatternId = Candidate->PatternId;
	RuntimeSnapshot.TelegraphRemainingSeconds = Candidate->Telegraph.DurationSeconds;
	TelegraphEndTime = Now + Candidate->Telegraph.DurationSeconds;
	RuntimeSnapshot.bInterruptible = Candidate->bInterruptible;
	CurrentTargets.Reset();
	LockedTargetLocations.Reset();
	for (APRCharacter* Target : SelectTargets(Candidate->Targeting))
	{
		CurrentTargets.Add(Target);
		LockedTargetLocations.Add(Target ? Target->GetActorLocation() : Boss->GetActorLocation());
	}
	if (!CurrentTargets.IsEmpty())
	{
		RuntimeSnapshot.PrimaryTargetId = FName(*GetNameSafe(CurrentTargets[0]->GetPlayerState()));
	}
	else
	{
		RuntimeSnapshot.PrimaryTargetId = NAME_None;
	}
	RecentPatterns.Add(Candidate->PatternId);
	while (RecentPatterns.Num() > BossDefinition->RecentPatternRepeatWindow) { RecentPatterns.RemoveAt(0); }
	NextPatternTimes.Add(Candidate->PatternId, Now + Candidate->CooldownSeconds * (Phase ? Phase->CooldownMultiplier : 1.0f));
	++DecisionOrdinal;
	FVector TelegraphLocation = Boss->GetActorLocation();
	if (Candidate->Telegraph.Origin == EPRBossTelegraphOrigin::PrimaryTarget && !CurrentTargets.IsEmpty() && CurrentTargets[0].IsValid())
	{
		TelegraphLocation = CurrentTargets[0]->GetActorLocation();
	}
	else if (Candidate->Telegraph.Origin == EPRBossTelegraphOrigin::LockedTargetPoint)
	{
		TelegraphLocation = GetLockedTargetLocation();
	}
	FRotator TelegraphRotation = FRotator::ZeroRotator;
	if (Candidate->Telegraph.Shape == EPRBossTelegraphShape::Line)
	{
		const FVector Direction = GetLockedTargetLocation() - Boss->GetActorLocation();
		TelegraphRotation = Direction.IsNearlyZero() ? Boss->GetActorRotation() : Direction.Rotation();
		TelegraphLocation = Boss->GetActorLocation() + Direction.GetSafeNormal() * (Candidate->Telegraph.Length * 0.5f);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Boss.Get();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveTelegraph = GetWorld()->SpawnActor<APRBossTelegraphActor>(APRBossTelegraphActor::StaticClass(), TelegraphLocation, TelegraphRotation, SpawnParameters);
	if (ActiveTelegraph.IsValid()) { ActiveTelegraph->InitializeTelegraph(Candidate->Telegraph, Candidate->PatternId); }
	if (APRBossCharacter* BossActor = Boss.Get())
	{
		BossActor->GetAbilitySystemComponent()->ExecuteGameplayCue(
			Candidate->Telegraph.CueTag.IsValid() ? Candidate->Telegraph.CueTag : ProjectRiftGameplayTags::GameplayCue_Boss_Telegraph_Warning);
	}
	GetWorld()->GetTimerManager().SetTimer(TelegraphTimer, this, &UPRBossSchedulerComponent::CompleteTelegraph, Candidate->Telegraph.DurationSeconds, false);
	PublishSnapshot();
}

const FPRBossAbilityPatternDefinition* UPRBossSchedulerComponent::GetActivePatternDefinition() const
{
	const UPRBossDefinitionDataAsset* BossDefinition = Definition.Get();
	return BossDefinition ? BossDefinition->AbilityPatterns.FindByPredicate([this](const FPRBossAbilityPatternDefinition& Candidate)
	{
		return Candidate.PatternId == RuntimeSnapshot.ActivePatternId;
	}) : nullptr;
}

void UPRBossSchedulerComponent::CompleteTelegraph()
{
	if (RuntimeSnapshot.State == EPRBossRuntimeState::Telegraphing)
	{
		if (ActiveTelegraph.IsValid()) { ActiveTelegraph->Destroy(); }
		ActiveTelegraph.Reset();
		RuntimeSnapshot.State = EPRBossRuntimeState::Executing;
		RuntimeSnapshot.TelegraphRemainingSeconds = 0.0f;
		TelegraphEndTime = 0.0f;
		if (APRBossCharacter* BossActor = Boss.Get())
		{
			if (UPRBossDefinitionDataAsset* BossDefinition = Definition.Get())
			{
				if (const FPRBossAbilityPatternDefinition* Pattern = BossDefinition->AbilityPatterns.FindByPredicate([this](const FPRBossAbilityPatternDefinition& Candidate)
					{ return Candidate.PatternId == RuntimeSnapshot.ActivePatternId; }))
				{
					if (Pattern->AbilityClass)
					{
						BossActor->GetAbilitySystemComponent()->TryActivateAbilityByClass(Pattern->AbilityClass);
					}
					GetWorld()->GetTimerManager().SetTimer(ExecutionTimer, this, &UPRBossSchedulerComponent::CompleteExecution, Pattern->ExecutionDurationSeconds, false);
				}
			}
		}
		PublishSnapshot();
	}
}

void UPRBossSchedulerComponent::CompleteExecution()
{
	if (RuntimeSnapshot.State == EPRBossRuntimeState::Executing)
	{
		RuntimeSnapshot.State = EPRBossRuntimeState::Selecting;
		RuntimeSnapshot.ActivePatternId = NAME_None;
		RuntimeSnapshot.bInterruptible = false;
		TelegraphEndTime = 0.0f;
		CurrentTargets.Reset();
		LockedTargetLocations.Reset();
		if (ActiveTelegraph.IsValid()) { ActiveTelegraph->Destroy(); }
		ActiveTelegraph.Reset();
		PublishSnapshot();
	}
}

void UPRBossSchedulerComponent::CompletePhaseTransition()
{
	if (RuntimeSnapshot.State == EPRBossRuntimeState::PhaseTransition)
	{
		RuntimeSnapshot.State = EPRBossRuntimeState::Selecting;
		if (APRBossCharacter* BossActor = Boss.Get())
		{
			const FPRBossPhaseDefinition* Phase = Definition.IsValid() && Definition->Phases.IsValidIndex(RuntimeSnapshot.PhaseIndex)
				? &Definition->Phases[RuntimeSnapshot.PhaseIndex] : nullptr;
			BossActor->GetAbilitySystemComponent()->ExecuteGameplayCue(Phase && Phase->PhaseCueTag.IsValid()
				? Phase->PhaseCueTag : ProjectRiftGameplayTags::GameplayCue_Boss_PhaseTransition);
		}
		PublishSnapshot();
	}
}

void UPRBossSchedulerComponent::ClearStagger()
{
	RuntimeSnapshot.State = EPRBossRuntimeState::Selecting;
	RuntimeSnapshot.bStaggered = false;
	RuntimeSnapshot.ActivePatternId = NAME_None;
	RuntimeSnapshot.bInterruptible = false;
	CurrentTargets.Reset();
	LockedTargetLocations.Reset();
	PublishSnapshot();
}

bool UPRBossSchedulerComponent::UpdatePhaseForHealthPercent()
{
	const UPRBossDefinitionDataAsset* BossDefinition = Definition.Get();
	if (!BossDefinition || RuntimeSnapshot.PhaseIndex == INDEX_NONE)
	{
		return false;
	}
	int32 DesiredPhase = RuntimeSnapshot.PhaseIndex;
	for (int32 Index = RuntimeSnapshot.PhaseIndex + 1; Index < BossDefinition->Phases.Num(); ++Index)
	{
		if (RuntimeSnapshot.HealthPercent <= BossDefinition->Phases[Index].StartHealthPercent)
		{
			DesiredPhase = Index;
		}
	}
	if (DesiredPhase == RuntimeSnapshot.PhaseIndex)
	{
		return false;
	}
	RuntimeSnapshot.PhaseIndex = DesiredPhase;
	RuntimeSnapshot.PhaseId = BossDefinition->Phases[DesiredPhase].PhaseId;
	RuntimeSnapshot.State = EPRBossRuntimeState::PhaseTransition;
	RuntimeSnapshot.ActivePatternId = NAME_None;
	RuntimeSnapshot.bInterruptible = false;
	WeakPointDamage = 0.0f;
	CurrentTargets.Reset();
	LockedTargetLocations.Reset();
	if (APRBossCharacter* BossActor = Boss.Get())
	{
		BossActor->ApplyPhaseModifiers(BossDefinition->Phases[DesiredPhase]);
	}
	GetWorld()->GetTimerManager().SetTimer(PhaseTransitionTimer, this, &UPRBossSchedulerComponent::CompletePhaseTransition, 0.5f, false);
	PublishSnapshot();
	return true;
}

FVector UPRBossSchedulerComponent::GetLockedTargetLocation(const int32 TargetIndex) const
{
	if (LockedTargetLocations.IsValidIndex(TargetIndex))
	{
		return LockedTargetLocations[TargetIndex];
	}
	return Boss.IsValid() ? Boss->GetActorLocation() : FVector::ZeroVector;
}

TArray<APRCharacter*> UPRBossSchedulerComponent::SelectTargets(const FPRBossTargetingDefinition& Targeting) const
{
	TArray<APRCharacter*> Candidates;
	const APRBossCharacter* BossActor = Boss.Get();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!BossActor || !GameState) { return Candidates; }
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APRCharacter* Candidate = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
		if (!Candidate || !Candidate->IsAlive()) { continue; }
		if (Targeting.MaximumRange > 0.0f && FVector::DistSquared(BossActor->GetActorLocation(), Candidate->GetActorLocation()) > FMath::Square(Targeting.MaximumRange)) { continue; }
		Candidates.Add(Candidate);
	}
	Candidates.Sort([](const APRCharacter& A, const APRCharacter& B) { return GetNameSafe(A.GetPlayerState()) < GetNameSafe(B.GetPlayerState()); });
	if (Candidates.IsEmpty()) { return Candidates; }
	const int32 DesiredCount = Targeting.Policy == EPRBossTargetPolicy::AllLiving
		? Candidates.Num()
		: FMath::Clamp(Targeting.BaseTargetCount + Targeting.AdditionalTargetsPerPlayer * (FrozenPlayerCount - 1), 1, Candidates.Num());
	if (Targeting.Policy == EPRBossTargetPolicy::DeterministicRandomUnique)
	{
		FRandomStream RandomStream(DecisionOrdinal * 1619 + FrozenPlayerCount * 31337);
		for (int32 Index = Candidates.Num() - 1; Index > 0; --Index) { Candidates.Swap(Index, RandomStream.RandRange(0, Index)); }
	}
	else if (Targeting.Policy != EPRBossTargetPolicy::AllLiving)
	{
		const UPREnemyThreatComponent* Threat = BossActor->GetEnemyThreatComponent();
		Candidates.Sort([BossActor, Threat, &Targeting](const APRCharacter& A, const APRCharacter& B)
		{
			const float ADistance = FVector::DistSquared(BossActor->GetActorLocation(), A.GetActorLocation());
			const float BDistance = FVector::DistSquared(BossActor->GetActorLocation(), B.GetActorLocation());
		if (Targeting.Policy == EPRBossTargetPolicy::Farthest) return ADistance > BDistance;
			if (Targeting.Policy == EPRBossTargetPolicy::Nearest) return ADistance < BDistance;
			if (Targeting.Policy == EPRBossTargetPolicy::HighestThreat) return (Threat ? Threat->GetThreatFor(&A) : 0.0f) > (Threat ? Threat->GetThreatFor(&B) : 0.0f);
			if (Targeting.Policy == EPRBossTargetPolicy::LowestHealth)
			{
				const UPRAttributeSet* AAttributes = A.GetAttributeSet();
				const UPRAttributeSet* BAttributes = B.GetAttributeSet();
				const float APercent = AAttributes && AAttributes->GetMaxHealth() > 0.0f ? AAttributes->GetHealth() / AAttributes->GetMaxHealth() : 1.0f;
				const float BPercent = BAttributes && BAttributes->GetMaxHealth() > 0.0f ? BAttributes->GetHealth() / BAttributes->GetMaxHealth() : 1.0f;
				return APercent < BPercent;
			}
			return ADistance < BDistance;
		});
	}
	Candidates.SetNum(DesiredCount);
	return Candidates;
}

void UPRBossSchedulerComponent::PublishSnapshot()
{
	if (APRBossCharacter* BossActor = Boss.Get())
	{
		RuntimeSnapshot.HealthPercent = BossActor->GetHealthPercent();
	}
	if (UWorld* World = GetWorld())
	{
		if (APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>())
		{
			RiftGameState->SetBossRuntimeSnapshot(RuntimeSnapshot);
		}
	}
}
