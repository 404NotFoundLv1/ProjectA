#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Deployables/PRDeployableTypes.h"
#include "PRDeployableActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class APRPlayerState;
class UPREngineerModuleDataAsset;

UCLASS(Abstract)
class PROJECTA_API APRDeployableActor : public AActor
{
	GENERATED_BODY()

public:
	APRDeployableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeDeployable(APRPlayerState* InOwnerPlayerState, EPRDeployableKind InKind, float InLifetimeSeconds);
	virtual void ConfigureFromModule(const UPREngineerModuleDataAsset* Module);

	UFUNCTION(BlueprintPure, Category = "Deployable")
	APRPlayerState* GetOwningPlayerState() const { return OwningPlayerState.Get(); }

	UFUNCTION(BlueprintPure, Category = "Deployable")
	EPRDeployableKind GetDeployableKind() const { return DeployableKind; }

	UFUNCTION(BlueprintPure, Category = "Deployable")
	float GetRemainingLifetimeSeconds() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployable")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployable")
	TObjectPtr<UTextRenderComponent> LabelComponent;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Deployable")
	EPRDeployableKind DeployableKind = EPRDeployableKind::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Deployable")
	TObjectPtr<APRPlayerState> OwningPlayerState;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Deployable")
	float ExpireAtServerTime = 0.0f;
};
