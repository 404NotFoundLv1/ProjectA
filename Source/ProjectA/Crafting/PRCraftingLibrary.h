#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Crafting/PRCraftingTypes.h"
#include "PRCraftingLibrary.generated.h"

class UPRCraftingRecipeDataAsset;
class UPRItemDataAsset;
class UPRAffixDefinitionDataAsset;
class UPREquipmentDataAsset;

/** Pure snapshot mutations shared by the authoritative runtime and local receipt verification. */
UCLASS()
class PROJECTA_API UPRCraftingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool ApplyRecipeToSnapshot(
		const UPRCraftingRecipeDataAsset* Recipe,
		const UPRItemDataAsset* OutputDefinition,
		int32 CraftSeed,
		const TArray<UPRAffixDefinitionDataAsset*>& AffixCatalog,
		FPRProfileSnapshot& InOutSnapshot,
		FPRItemInstance& OutCraftedItem,
		FPRCraftingEvaluation& OutEvaluation);

	/** Dismantles only a backpack instance. Equipped instances must first be explicitly unequipped. */
	static bool ApplyDismantleToSnapshot(
		const FGuid& InstanceGuid,
		const UPRItemDataAsset* ItemDefinition,
		FPRProfileSnapshot& InOutSnapshot,
		FPRCraftingEvaluation& OutEvaluation);

	/** Advances one concrete equipment instance by one bounded level and charges its authored cost. */
	static bool ApplyUpgradeToSnapshot(
		const FGuid& InstanceGuid,
		const UPREquipmentDataAsset* EquipmentDefinition,
		FPRProfileSnapshot& InOutSnapshot,
		FPRCraftingEvaluation& OutEvaluation);
};
