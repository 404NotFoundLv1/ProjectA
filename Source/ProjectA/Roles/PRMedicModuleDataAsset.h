#pragma once

#include "CoreMinimal.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "PRMedicModuleDataAsset.generated.h"

UENUM(BlueprintType)
enum class EPRMedicModuleKind : uint8
{
	None,
	FieldHeal,
	PurificationPulse,
	ReconScan
};

/** Data-driven parameters for the three Medic role modules. */
UCLASS(BlueprintType)
class PROJECTA_API UPRMedicModuleDataAsset : public UPRRoleModuleDataAsset
{
	GENERATED_BODY()

public:
	virtual bool ValidateDefinition(FString* OutDiagnostic = nullptr) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Medic")
	EPRMedicModuleKind MedicKind = EPRMedicModuleKind::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Medic", meta = (ClampMin = "0.0"))
	float PrimaryMagnitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Medic", meta = (ClampMin = "0.0"))
	float TargetRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Medic", meta = (ClampMin = "0.0"))
	float EffectRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Medic", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;
};
