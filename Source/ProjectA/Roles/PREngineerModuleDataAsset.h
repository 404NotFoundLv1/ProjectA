#pragma once

#include "CoreMinimal.h"
#include "Deployables/PRDeployableTypes.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "PREngineerModuleDataAsset.generated.h"

class APRDeployableActor;

UCLASS(BlueprintType)
class PROJECTA_API UPREngineerModuleDataAsset : public UPRRoleModuleDataAsset
{
	GENERATED_BODY()

public:
	virtual bool ValidateDefinition(FString* OutDiagnostic = nullptr) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Deployable")
	EPRDeployableKind DeployableKind = EPRDeployableKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Deployable")
	TSubclassOf<APRDeployableActor> DeployableActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Placement", meta = (ClampMin = "1.0"))
	float PlacementRange = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Placement", meta = (ClampMin = "1.0"))
	float FootprintRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Placement", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSurfaceSlopeDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Runtime", meta = (ClampMin = "0.01"))
	float LifetimeSeconds = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Runtime", meta = (ClampMin = "0.0"))
	float EffectRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Runtime", meta = (ClampMin = "0.01"))
	float EffectIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engineer|Runtime", meta = (ClampMin = "0.0"))
	float PrimaryMagnitude = 0.0f;
};
