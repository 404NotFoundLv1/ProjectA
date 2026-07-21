#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PREnemyBehaviorComponent.generated.h"

UCLASS(ClassGroup=(Enemy), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTA_API UPREnemyBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPREnemyBehaviorComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartBehavior();
	void StopBehavior();
	class APRCharacter* GetCurrentTarget() const { return CurrentTarget.Get(); }

private:
	void EvaluateBehavior();
	void ExecutePendingAction();
	int32 SelectUsableAction(class APREnemyCharacter* Enemy, class APRCharacter* Target, float CurrentTime) const;
	bool ExecuteAction(class APREnemyCharacter* Enemy, class APRCharacter* Target, const struct FPREnemyActionDefinition& Action);
	void ApplyActionToTarget(class APREnemyCharacter* Enemy, class APRCharacter* Target, const struct FPREnemyActionDefinition& Action, int32 Repetitions = 1);
	bool HasProjectileLineOfSight(const class APREnemyCharacter* Enemy, const class APRCharacter* Target) const;
	bool RepositionForMobilityAction(class APREnemyCharacter* Enemy, const class APRCharacter* Target, const struct FPREnemyActionDefinition& Action) const;

	FTimerHandle EvaluationTimer;
	FTimerHandle TelegraphTimer;
	TWeakObjectPtr<class APRCharacter> CurrentTarget;
	TWeakObjectPtr<class APRCharacter> PendingTarget;
	int32 PendingActionIndex = INDEX_NONE;
	bool bTelegraphPending = false;
	TMap<FGameplayTag, float> NextActionTimes;
};
