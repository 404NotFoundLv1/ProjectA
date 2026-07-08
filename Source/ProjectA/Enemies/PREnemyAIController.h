#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "PREnemyAIController.generated.h"

class APRCharacter;
class APREnemyCharacter;

/**
 * Minimal server-side AI for the first ProjectRift melee enemy.
 */
UCLASS()
class PROJECTA_API APREnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	APREnemyAIController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|AI")
	bool RefreshTarget();

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	APawn* GetCurrentTarget() const { return CurrentTarget.Get(); }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI", meta = (ClampMin = "0.1"))
	float TargetRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|AI", meta = (ClampMin = "0.1"))
	float MeleeAttackInterval = 1.0f;

private:
	void HandleTargetRefresh();
	APRCharacter* FindNearestLivingPlayer() const;
	bool TryAttackCurrentTarget();

	UPROPERTY(Transient)
	TObjectPtr<APawn> CurrentTarget;

	float TimeSinceTargetRefresh = 0.0f;
	float TimeSinceLastMeleeAttack = 0.0f;
};
