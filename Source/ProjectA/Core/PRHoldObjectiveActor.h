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
	float GetHoldDuration() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	float GetCurrentHoldTime() const { return CurrentHoldTime; }

protected:
	virtual void HandleObjectiveActivated(AController* ActivatingController) override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHoldTime, BlueprintReadOnly, Category = "Rift|Objective")
	float CurrentHoldTime = 0.0f;

	UFUNCTION()
	void OnRep_CurrentHoldTime();

	/** Zero preserves the project default; graph test maps use a 15-second instance override. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Rift|Objective", meta = (ClampMin = "0.0"))
	float HoldDurationOverride = 0.0f;

private:
	void SetCurrentHoldTime(float InCurrentHoldTime);
};
