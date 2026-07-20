#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PREquipmentDataAsset.h"
#include "Crafting/PRCraftingRecipeDataAsset.h"
#include "Crafting/PRCraftingLibrary.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCraftingContractTest,
	"ProjectRift.Crafting.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRCraftingContractTest::RunTest(const FString& Parameters)
{
	UClass* RecipeClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRCraftingRecipeDataAsset"));
	TestNotNull(TEXT("Crafting recipe definition class exists"), RecipeClass);
	if (RecipeClass)
	{
		TestNotNull(TEXT("Recipe exposes its stable id"), FindFProperty<FNameProperty>(RecipeClass, TEXT("RecipeId")));
		TestNotNull(TEXT("Recipe exposes its resource costs"), FindFProperty<FArrayProperty>(RecipeClass, TEXT("ResourceCosts")));
		TestNotNull(TEXT("Recipe exposes its output item id"), FindFProperty<FNameProperty>(RecipeClass, TEXT("OutputItemId")));
		TestNotNull(TEXT("Recipe exposes its output count"), FindFProperty<FIntProperty>(RecipeClass, TEXT("OutputCount")));
		TestNotNull(TEXT("Recipe exposes story unlock requirements"), FindFProperty<FArrayProperty>(RecipeClass, TEXT("RequiredCompletedStoryNodeIds")));
		TestNotNull(TEXT("Recipe exposes module unlock id"), FindFProperty<FNameProperty>(RecipeClass, TEXT("RequiredShipModuleId")));
		TestNotNull(TEXT("Recipe exposes module unlock level"), FindFProperty<FIntProperty>(RecipeClass, TEXT("RequiredShipModuleLevel")));
		TestNotNull(TEXT("Recipe exposes fixed equipment rarity"), FindFProperty<FEnumProperty>(RecipeClass, TEXT("EquipmentRarity")));
		TestNotNull(TEXT("Recipe exposes fixed equipment affix count"), FindFProperty<FIntProperty>(RecipeClass, TEXT("EquipmentAffixCount")));
		TestNotNull(TEXT("Recipe exposes validation"), RecipeClass->FindFunctionByName(TEXT("IsRecipeValid")));
	}

	UClass* StationClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRCraftingStation"));
	TestNotNull(TEXT("Crafting station class exists"), StationClass);
	TestNotNull(TEXT("Crafting widget class exists"), FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRCraftingWidget")));
	TestNotNull(TEXT("Profile snapshot has durable crafting replay ledger"), FindFProperty<FArrayProperty>(FPRProfileSnapshot::StaticStruct(), TEXT("ProcessedCraftingTransactionIds")));

	UClass* ItemClass = UPRItemDataAsset::StaticClass();
	TestNotNull(TEXT("Items can declare dismantle eligibility"), FindFProperty<FBoolProperty>(ItemClass, TEXT("bCanDismantle")));
	TestNotNull(TEXT("Items can declare dismantle results"), FindFProperty<FStructProperty>(ItemClass, TEXT("DismantleResult")));

	UClass* EquipmentClass = UPREquipmentDataAsset::StaticClass();
	TestNotNull(TEXT("Equipment declares a bounded upgrade level"), FindFProperty<FIntProperty>(EquipmentClass, TEXT("MaxUpgradeLevel")));
	TestNotNull(TEXT("Equipment declares upgrade costs"), FindFProperty<FArrayProperty>(EquipmentClass, TEXT("UpgradeCosts")));
	TestNotNull(TEXT("Equipment declares per-level upgrade modifiers"), FindFProperty<FArrayProperty>(EquipmentClass, TEXT("UpgradeModifiersPerLevel")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCraftingPolicyTest,
	"ProjectRift.Crafting.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRCraftingPolicyTest::RunTest(const FString& Parameters)
{
	UPRCraftingRecipeDataAsset* Recipe = NewObject<UPRCraftingRecipeDataAsset>(GetTransientPackage());
	Recipe->RecipeId = TEXT("Recipe.Test.HealthInjector");
	Recipe->DisplayName = FText::FromString(TEXT("Test injector"));
	Recipe->OutputItemId = TEXT("HealthInjector");
	Recipe->OutputCount = 1;
	Recipe->ResourceCosts = { FPRCraftingResourceCost{ TEXT("EnergyCrystal"), 2 } };

	UPRItemDataAsset* OutputDefinition = NewObject<UPRItemDataAsset>(GetTransientPackage());
	OutputDefinition->ItemId = Recipe->OutputItemId;
	OutputDefinition->ItemType = EPRItemType::Consumable;
	OutputDefinition->MaxStackCount = 5;

	FPRProfileSnapshot InsufficientSnapshot;
	InsufficientSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 1 } };
	const FPRProfileSnapshot OriginalSnapshot = InsufficientSnapshot;
	FPRItemInstance CraftedItem;
	FPRCraftingEvaluation Evaluation;
	TestFalse(TEXT("Insufficient resources reject crafting"), UPRCraftingLibrary::ApplyRecipeToSnapshot(Recipe, OutputDefinition, 17, {}, InsufficientSnapshot, CraftedItem, Evaluation));
	TestEqual(TEXT("Rejected crafting retains all resources"), InsufficientSnapshot.GetResourceCount(TEXT("EnergyCrystal")), OriginalSnapshot.GetResourceCount(TEXT("EnergyCrystal")));
	TestTrue(TEXT("Rejected crafting creates no item"), InsufficientSnapshot.BackpackItems.IsEmpty());

	FPRProfileSnapshot ExactSnapshot;
	ExactSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 2 } };
	TestTrue(TEXT("Exact resources craft atomically"), UPRCraftingLibrary::ApplyRecipeToSnapshot(Recipe, OutputDefinition, 17, {}, ExactSnapshot, CraftedItem, Evaluation));
	TestEqual(TEXT("Crafting removes exact resource cost"), ExactSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 0);
	TestEqual(TEXT("Crafting produces the configured output"), ExactSnapshot.BackpackItems.Num(), 1);
	TestEqual(TEXT("Crafted output uses configured item id"), CraftedItem.ItemId, FName(TEXT("HealthInjector")));
	TestTrue(TEXT("Crafted output has a server identity"), CraftedItem.HasValidIdentity());

	UPREquipmentDataAsset* Equipment = NewObject<UPREquipmentDataAsset>(GetTransientPackage());
	Equipment->ItemId = TEXT("Item.TestCraftEquipment");
	Equipment->bCanDismantle = true;
	Equipment->DismantleResult.ResourceReturns = { FPRCraftingResourceCost{ TEXT("EnergyCrystal"), 3 } };
	Equipment->MaxUpgradeLevel = 3;
	FPRUpgradeCost UpgradeCost;
	UpgradeCost.TargetLevel = 2;
	UpgradeCost.ResourceCosts = { FPRCraftingResourceCost{ TEXT("EnergyCrystal"), 2 } };
	Equipment->UpgradeCosts = { UpgradeCost };
	FPRItemInstance Target;
	Target.InstanceGuid = FGuid::NewGuid();
	Target.ItemId = Equipment->ItemId;
	Target.Count = 1;
	Target.Level = 1;
	FPRProfileSnapshot UpgradeSnapshot;
	UpgradeSnapshot.BackpackItems = { Target };
	UpgradeSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 2 } };
	TestTrue(TEXT("Upgrade debits authored cost and advances one bounded level"), UPRCraftingLibrary::ApplyUpgradeToSnapshot(Target.InstanceGuid, Equipment, UpgradeSnapshot, Evaluation));
	TestEqual(TEXT("Upgrade advances target to level two"), UpgradeSnapshot.BackpackItems[0].Level, 2);
	TestEqual(TEXT("Upgrade consumes exact resources"), UpgradeSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 0);
	TestTrue(TEXT("Dismantle removes only an owned backpack instance"), UPRCraftingLibrary::ApplyDismantleToSnapshot(Target.InstanceGuid, Equipment, UpgradeSnapshot, Evaluation));
	TestTrue(TEXT("Dismantle removes equipment instance"), UpgradeSnapshot.BackpackItems.IsEmpty());
	TestEqual(TEXT("Dismantle returns explicit authored resources"), UpgradeSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 3);

	return true;
}

#endif
