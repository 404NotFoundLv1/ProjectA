#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "Roles/PRRoleTypes.h"
#include "PRRoleModuleDataAsset.generated.h"

UCLASS(BlueprintType)
class PROJECTA_API UPRRoleModuleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType RoleModulePrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module")
	FName ModuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module")
	FName RoleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module")
	EPRRoleModuleSlot Slot = EPRRoleModuleSlot::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module")
	TSubclassOf<UGameplayAbility> GameplayAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module", meta = (ClampMin = "0.0"))
	float EnergyCost = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role Module", meta = (MultiLine = "true"))
	FText Description;

	virtual bool ValidateDefinition(FString* OutDiagnostic = nullptr) const;
};
