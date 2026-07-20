#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Crafting/PRCraftingTypes.h"
#include "Items/PRItemTypes.h"
#include "PRCraftingRecipeDataAsset.generated.h"

/** Data-driven crafting contract. v0.7.5 extends this with costs, output, and unlock requirements. */
UCLASS(BlueprintType)
class PROJECTA_API UPRCraftingRecipeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType CraftingRecipePrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsRecipeValid() const;

	bool ValidateRecipe(FString& OutDiagnostic) const;
	static bool ValidateCatalog(const TArray<UPRCraftingRecipeDataAsset*>& Recipes, FString& OutDiagnostic);
	FPRCraftingEvaluation EvaluateRecipe(const FPRProfileSnapshot& Snapshot) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (AssetRegistrySearchable))
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TArray<FPRCraftingResourceCost> ResourceCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName OutputItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 OutputCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Equipment")
	EPRItemRarity EquipmentRarity = EPRItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Equipment", meta = (ClampMin = "0", ClampMax = "3"))
	int32 EquipmentAffixCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Unlock")
	TArray<FName> RequiredCompletedStoryNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Unlock")
	FName RequiredChapterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Unlock")
	FName RequiredShipModuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Unlock", meta = (ClampMin = "0"))
	int32 RequiredShipModuleLevel = 0;
};
