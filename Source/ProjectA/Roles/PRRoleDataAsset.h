#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Roles/PRRoleTypes.h"
#include "PRRoleDataAsset.generated.h"

class UPRRoleModuleDataAsset;

UCLASS(BlueprintType)
class PROJECTA_API UPRRoleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType RolePrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	FName RoleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	TArray<FName> AllowedModuleIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	FPRRoleLoadout DefaultLoadout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role", meta = (ClampMin = "0.001"))
	float EnergyRegenPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role")
	bool bStarterUnlocked = false;

	bool ValidateDefinition(FString* OutDiagnostic = nullptr) const;
	static bool ValidateCatalog(
		const TArray<UPRRoleDataAsset*>& Roles,
		const TArray<UPRRoleModuleDataAsset*>& Modules,
		FString* OutDiagnostic = nullptr);
};
