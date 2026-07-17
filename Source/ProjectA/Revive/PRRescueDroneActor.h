#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Revive/PRReviveTypes.h"
#include "PRRescueDroneActor.generated.h"

class APRCharacter;
class UStaticMeshComponent;

/** Non-combat, replicated solo safety-net drone. */
UCLASS()
class PROJECTA_API APRRescueDroneActor : public AActor
{
	GENERATED_BODY()

public:
	APRRescueDroneActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool InitializeForPlayer(APRCharacter* InOwnerCharacter);
	bool RequestRescue(APRCharacter* DownedCharacter);
	void ShutdownDrone();

	UFUNCTION(BlueprintPure, Category = "Revive|Drone")
	EPRRescueDroneState GetDroneState() const { return DroneState; }

	UFUNCTION(BlueprintPure, Category = "Revive|Drone")
	bool CanProvideRecovery() const { return DroneState == EPRRescueDroneState::Ready || DroneState == EPRRescueDroneState::Deployed; }

	UFUNCTION(BlueprintPure, Category = "Revive|Drone")
	APRCharacter* GetAssignedPlayer() const { return AssignedPlayer; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Revive|Drone")
	TObjectPtr<UStaticMeshComponent> DroneMesh;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive|Drone")
	EPRRescueDroneState DroneState = EPRRescueDroneState::Unavailable;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive|Drone")
	TObjectPtr<APRCharacter> AssignedPlayer;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive|Drone")
	TObjectPtr<APRCharacter> RescueTarget;

	float RescueStartServerTime = 0.0f;
};
