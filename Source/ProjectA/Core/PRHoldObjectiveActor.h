#pragma once

#include "CoreMinimal.h"
#include "Core/PRRiftObjectiveActor.h"
#include "PRHoldObjectiveActor.generated.h"

/**
 * First playable rift objective: hold a point until the timer completes.
 */
UCLASS()
class PROJECTA_API APRHoldObjectiveActor : public APRRiftObjectiveActor
{
	GENERATED_BODY()

public:
	APRHoldObjectiveActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	float GetHoldDuration() const { return HoldDuration; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	float GetCurrentHoldTime() const { return CurrentHoldTime; }

protected:
	virtual void HandleObjectiveActivated(AController* ActivatingController) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Objective", meta = (ClampMin = "0.1"))
	float HoldDuration = 30.0f;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHoldTime, BlueprintReadOnly, Category = "Rift|Objective")
	float CurrentHoldTime = 0.0f;

	UFUNCTION()
	void OnRep_CurrentHoldTime();

private:
	void SetCurrentHoldTime(float InCurrentHoldTime);
};
