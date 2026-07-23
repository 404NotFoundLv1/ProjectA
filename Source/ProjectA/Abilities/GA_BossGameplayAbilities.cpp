#include "Abilities/GA_BossGameplayAbilities.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Bosses/PRBossCharacter.h"
#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Bosses/PRBossSchedulerComponent.h"
#include "Characters/PRCharacter.h"
#include "Core/PRSpawnManager.h"
#include "EngineUtils.h"

namespace
{
	void ApplyPatternDamage(APRBossCharacter* Boss, const bool bUseRadius)
	{
		if (!Boss || !Boss->HasAuthority()) { return; }
		const UPRBossSchedulerComponent* Scheduler = Boss->GetBossScheduler();
		const FPRBossAbilityPatternDefinition* Pattern = Scheduler ? Scheduler->GetActivePatternDefinition() : nullptr;
		UPRAbilitySystemComponent* SourceASC = Boss->GetProjectRiftAbilitySystemComponent();
		if (!Pattern || !SourceASC || Pattern->BaseDamage <= 0.0f) { return; }
		for (const TWeakObjectPtr<APRCharacter>& Target : Scheduler->GetCurrentTargets())
		{
			APRCharacter* TargetCharacter = Target.Get();
			if (!TargetCharacter || !TargetCharacter->IsAlive() || (bUseRadius && Pattern->EffectRadius > 0.0f && FVector::DistSquared(Boss->GetActorLocation(), TargetCharacter->GetActorLocation()) > FMath::Square(Pattern->EffectRadius))) { continue; }
			UPRAbilitySystemComponent* TargetASC = TargetCharacter->GetProjectRiftAbilitySystemComponent();
			if (!TargetASC) { continue; }
			FPRDamageRequest Request;
			Request.BaseDamage = Pattern->BaseDamage;
			Request.DamageType = Pattern->DamageType;
			Request.HitReaction = Pattern->HitReaction;
			Request.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
			Request.HitResult.Location = TargetCharacter->GetActorLocation();
			Request.HitResult.ImpactPoint = Request.HitResult.Location;
			UPRCombatEffectLibrary::ApplyDamageRequestToTarget(SourceASC, TargetASC, Request, Boss);
			for (const FPRTargetStatusEffectDefinition& Status : Pattern->TargetStatusEffects)
			{
				UPRCombatEffectLibrary::ApplyStatusEffectToTarget(SourceASC, TargetASC, Status, Boss);
			}
		}
	}

	const FPRBossAbilityPatternDefinition* GetActivePattern(const APRBossCharacter* Boss)
	{
		return Boss && Boss->GetBossScheduler() ? Boss->GetBossScheduler()->GetActivePatternDefinition() : nullptr;
	}

	APRSpawnManager* FindNearestSpawnManager(const APRBossCharacter* Boss)
	{
		if (!Boss || !Boss->GetWorld()) { return nullptr; }
		APRSpawnManager* Result = nullptr;
		float BestDistanceSquared = TNumericLimits<float>::Max();
		for (TActorIterator<APRSpawnManager> It(Boss->GetWorld()); It; ++It)
		{
			const float DistanceSquared = FVector::DistSquared(Boss->GetActorLocation(), It->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				Result = *It;
				BestDistanceSquared = DistanceSquared;
			}
		}
		return Result;
	}
}

void UGA_BossTargetedDamage::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	ApplyPatternDamage(ActorInfo ? Cast<APRBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr, false);
}

void UGA_BossRadialDamage::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	ApplyPatternDamage(ActorInfo ? Cast<APRBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr, true);
}

void UGA_BossSummon::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	APRBossCharacter* Boss = ActorInfo ? Cast<APRBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const FPRBossAbilityPatternDefinition* Pattern = GetActivePattern(Boss);
	if (!Boss || !Boss->HasAuthority() || !Pattern || (Pattern->SummonCount <= 0 && Pattern->AdditionalSummonsPerPlayer <= 0) || Pattern->SummonDefinitionId.IsNone()) { return; }
	if (APRSpawnManager* SpawnManager = FindNearestSpawnManager(Boss))
	{
		const int32 PlayerCount = Boss->GetBossScheduler() ? Boss->GetBossScheduler()->GetFrozenPlayerCount() : 1;
		const int32 RequestedCount = Pattern->SummonCount + Pattern->AdditionalSummonsPerPlayer * FMath::Max(0, PlayerCount - 1);
		SpawnManager->SpawnSummonedEnemies(Pattern->SummonDefinitionId, FMath::Min(RequestedCount, Pattern->MaxSummons), Boss);
	}
}

void UGA_BossArenaEvent::ExecuteBossPattern(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo, const FGameplayEventData*)
{
	APRBossCharacter* Boss = ActorInfo ? Cast<APRBossCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const FPRBossAbilityPatternDefinition* Pattern = GetActivePattern(Boss);
	const UPRBossDefinitionDataAsset* Definition = Boss ? Boss->GetBossDefinition() : nullptr;
	UPRAbilitySystemComponent* AbilitySystem = Boss ? Boss->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!Boss || !Boss->HasAuthority() || !Pattern || Pattern->ArenaEventId.IsNone() || !Definition || !AbilitySystem) { return; }
	const FPRBossArenaEventDefinition* ArenaEvent = Definition->ArenaEvents.FindByPredicate([Pattern](const FPRBossArenaEventDefinition& Candidate)
	{
		return Candidate.ArenaEventId == Pattern->ArenaEventId;
	});
	if (!ArenaEvent || !ArenaEvent->EventTag.IsValid()) { return; }
	FGameplayEventData EventData;
	EventData.EventTag = ArenaEvent->EventTag;
	EventData.EventMagnitude = ArenaEvent->DurationSeconds;
	EventData.Instigator = Boss;
	EventData.Target = Boss;
	AbilitySystem->HandleGameplayEvent(ArenaEvent->EventTag, &EventData);
}
