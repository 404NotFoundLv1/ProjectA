#pragma once

#include "Bosses/PRBossTypes.h"
#include "Components/ActorComponent.h"
#include "PRBossSchedulerComponent.generated.h"

class APRBossCharacter;
class APRCharacter;
class APRBossTelegraphActor;
class UPRBossDefinitionDataAsset;

/** Server-owned, timer-driven scheduler. Clients receive only its compact runtime snapshot. */
UCLASS(ClassGroup=(Boss), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTA_API UPRBossSchedulerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRBossSchedulerComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Scheduler")
	bool StartScheduler(APRBossCharacter* InBoss, UPRBossDefinitionDataAsset* InDefinition, int32 InFrozenPlayerCount);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Scheduler") void StopScheduler();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Scheduler") void ResetScheduler();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Scheduler") void NotifyWeakPointDamage(FName WeakPointId, float AppliedDamage);
	UFUNCTION(BlueprintPure, Category = "Boss|Scheduler") FPRBossRuntimeSnapshot GetRuntimeSnapshot() const { return RuntimeSnapshot; }
	const TArray<TWeakObjectPtr<APRCharacter>>& GetCurrentTargets() const { return CurrentTargets; }
	FVector GetLockedTargetLocation(int32 TargetIndex = 0) const;
	int32 GetFrozenPlayerCount() const { return FrozenPlayerCount; }
	const FPRBossAbilityPatternDefinition* GetActivePatternDefinition() const;
	bool IsWeakPointExposed() const { return RuntimeSnapshot.State == EPRBossRuntimeState::Executing && RuntimeSnapshot.bInterruptible; }

private:
	void EvaluateScheduler();
	void SelectNextPattern();
	void CompleteTelegraph();
	void CompleteExecution();
	void CompletePhaseTransition();
	void ClearStagger();
	bool UpdatePhaseForHealthPercent();
	TArray<APRCharacter*> SelectTargets(const FPRBossTargetingDefinition& Targeting) const;
	void PublishSnapshot();

	FTimerHandle EvaluationTimer;
	FTimerHandle TelegraphTimer;
	FTimerHandle StaggerTimer;
	FTimerHandle ExecutionTimer;
	FTimerHandle PhaseTransitionTimer;
	TWeakObjectPtr<APRBossCharacter> Boss;
	TWeakObjectPtr<UPRBossDefinitionDataAsset> Definition;
	FPRBossRuntimeSnapshot RuntimeSnapshot;
	TMap<FName, float> NextPatternTimes;
	TArray<FName> RecentPatterns;
	TArray<TWeakObjectPtr<APRCharacter>> CurrentTargets;
	TArray<FVector> LockedTargetLocations;
	TWeakObjectPtr<APRBossTelegraphActor> ActiveTelegraph;
	int32 FrozenPlayerCount = 1;
	int32 DecisionOrdinal = 0;
	float WeakPointDamage = 0.0f;
	float TelegraphEndTime = 0.0f;
};
