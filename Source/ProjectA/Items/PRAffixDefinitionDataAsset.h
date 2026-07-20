#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Items/PREquipmentTypes.h"
#include "Items/PRItemTypes.h"
#include "PRAffixDefinitionDataAsset.generated.h"

class UGameplayEffect;

/** Authorable, localized definition for one deterministic equipment affix. */
UCLASS(BlueprintType)
class PROJECTA_API UPRAffixDefinitionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AffixPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "Affix")
	bool ValidateDefinition(FString& OutDiagnostic) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix", meta = (AssetRegistrySearchable))
	FName AffixId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix")
	TArray<EPREquipmentSlot> AllowedSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix")
	EPRAffixAttribute Attribute = EPRAffixAttribute::MaxHealth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix")
	EPRAffixModifierType ModifierType = EPRAffixModifierType::Additive;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix", meta = (ClampMin = "0.0001"))
	float MinMagnitude = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix", meta = (ClampMin = "0.0001"))
	float MaxMagnitude = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix", meta = (ClampMin = "0.0001"))
	float Weight = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix")
	FGameplayTagContainer MutuallyExclusiveTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Affix|GAS")
	TSubclassOf<UGameplayEffect> ModifierEffectClass;
};

