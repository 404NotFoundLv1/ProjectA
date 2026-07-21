#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PRMissionModifierDefinitionDataAsset.generated.h"

class UGameplayEffect;
class UTexture2D;

UCLASS(BlueprintType)
class PROJECTA_API UPRMissionModifierDefinitionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ModifierPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool IsValidModifierDefinition(FString* OutDiagnostic = nullptr) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FGameplayTagContainer GrantedRuleTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	TSubclassOf<UGameplayEffect> PlayerEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	TSubclassOf<UGameplayEffect> EnemyEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float StabilityDrainMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float EnemyBudgetMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float EliteHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float EliteAttackMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float PollutionDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float RewardMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float WorldMaterialLootMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;
};
