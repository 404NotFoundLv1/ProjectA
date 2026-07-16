#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Deployables/PRDeployableTypes.h"
#include "PRDeployableComponent.generated.h"

class APRDeployableActor;
class APlayerController;
class UPREngineerModuleDataAsset;

UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRDeployableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRDeployableComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static bool IsFinitePlacementTransform(const FTransform& Transform);

	UFUNCTION(BlueprintPure, Category = "Deployable")
	const FPRDeployablePlacementState& GetPlacementState() const { return PlacementState; }

	UFUNCTION(BlueprintPure, Category = "Deployable")
	FString GetLastPlacementFailure() const { return LastPlacementFailure; }

	bool BeginPlacement(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic);
	EPRDeployablePlacementResult ConfirmPlacement(APlayerController* Controller, const FTransform& ClientTransform, int32 SessionSequence, FString& OutDiagnostic);
	void CancelPlacement(const FString& Reason = FString());

	UFUNCTION(BlueprintPure, Category = "Deployable")
	APRDeployableActor* GetActiveDeployable(EPRDeployableKind Kind) const;

	void ClearAllDeployables();

private:
	bool CanBeginPlacement(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic) const;
	bool ResolveAuthoritativePlacement(APlayerController* Controller, const UPREngineerModuleDataAsset* Module, const FTransform& ClientTransform, FTransform& OutTransform, EPRDeployablePlacementResult& OutResult, FString& OutDiagnostic) const;
	bool CommitModuleCostAndCooldown(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic) const;
	void SetPlacementStateActive(bool bActive);
	void SetActiveDeployable(EPRDeployableKind Kind, APRDeployableActor* Deployable);
	void HandlePlacementTimeout();
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Deployable")
	FPRDeployablePlacementState PlacementState;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Deployable")
	FString LastPlacementFailure;

	UPROPERTY(Transient)
	TObjectPtr<const UPREngineerModuleDataAsset> PendingModule;

	FTimerHandle PlacementTimeoutHandle;
	bool bConfirmInFlight = false;

	UPROPERTY(Replicated)
	TObjectPtr<APRDeployableActor> ActiveSentry;

	UPROPERTY(Replicated)
	TObjectPtr<APRDeployableActor> ActiveRepairDrone;

	UPROPERTY(Replicated)
	TObjectPtr<APRDeployableActor> ActiveShieldGenerator;
};
