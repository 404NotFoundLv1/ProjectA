#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PRAffixGenerationLibrary.h"
#include "Items/PRItemTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRAffixContractTest,
	"ProjectRift.Items.Affixes.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
template <typename PropertyType>
PropertyType* RequireAffixProperty(FAutomationTestBase& Test, UStruct* Owner, const TCHAR* Name)
{
	PropertyType* Property = Owner ? FindFProperty<PropertyType>(Owner, Name) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exists"), Name), Property);
	return Property;
}
}

bool FPRAffixContractTest::RunTest(const FString& Parameters)
{
	UEnum* RarityEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRItemRarity"));
	TestNotNull(TEXT("Item rarity enum is reflected"), RarityEnum);
	TestEqual(TEXT("Epic preserves its serialized value"), RarityEnum ? RarityEnum->GetValueByNameString(TEXT("Epic")) : INDEX_NONE, 3LL);
	TestEqual(TEXT("Legendary preserves its serialized value"), RarityEnum ? RarityEnum->GetValueByNameString(TEXT("Legendary")) : INDEX_NONE, 4LL);
	TestEqual(TEXT("Prototype is appended after legacy rarity values"), RarityEnum ? RarityEnum->GetValueByNameString(TEXT("Prototype")) : INDEX_NONE, 5LL);

	UScriptStruct* ItemInstanceStruct = FPRItemInstance::StaticStruct();
	RequireAffixProperty<FIntProperty>(*this, ItemInstanceStruct, TEXT("LootSeed"));
	RequireAffixProperty<FIntProperty>(*this, ItemInstanceStruct, TEXT("AffixGenerationVersion"));
	RequireAffixProperty<FArrayProperty>(*this, ItemInstanceStruct, TEXT("RolledAffixes"));

	UScriptStruct* RollStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRAffixRoll"));
	TestNotNull(TEXT("Final affix roll is reflected"), RollStruct);
	RequireAffixProperty<FNameProperty>(*this, RollStruct, TEXT("AffixId"));
	RequireAffixProperty<FFloatProperty>(*this, RollStruct, TEXT("Magnitude"));

	UClass* AffixDefinitionClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRAffixDefinitionDataAsset"));
	TestNotNull(TEXT("Affix definition data asset is reflected"), AffixDefinitionClass);
	RequireAffixProperty<FArrayProperty>(*this, AffixDefinitionClass, TEXT("AllowedSlots"));
	RequireAffixProperty<FFloatProperty>(*this, AffixDefinitionClass, TEXT("MinMagnitude"));
	RequireAffixProperty<FFloatProperty>(*this, AffixDefinitionClass, TEXT("MaxMagnitude"));
	RequireAffixProperty<FFloatProperty>(*this, AffixDefinitionClass, TEXT("Weight"));

	UClass* GenerationLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRAffixGenerationLibrary"));
	TestNotNull(TEXT("Affix generation library is reflected"), GenerationLibraryClass);
	TestNotNull(TEXT("Seeded equipment generation entry exists"), GenerationLibraryClass ? GenerationLibraryClass->FindFunctionByName(TEXT("GenerateEquipmentInstance")) : nullptr);

	const TArray<FPRRarityGenerationRule> DefaultRules = UPRAffixGenerationLibrary::MakeDefaultRarityRules();
	TestEqual(TEXT("Exactly four generated rarity rules are configured"), DefaultRules.Num(), 4);
	FString RuleDiagnostic;
	TestTrue(TEXT("Default rarity rules are valid"), UPRAffixGenerationLibrary::ValidateRarityRules(DefaultRules, RuleDiagnostic));
	const FPRRarityGenerationRule* Common = DefaultRules.FindByPredicate([](const FPRRarityGenerationRule& Rule) { return Rule.Rarity == EPRItemRarity::Common; });
	const FPRRarityGenerationRule* Uncommon = DefaultRules.FindByPredicate([](const FPRRarityGenerationRule& Rule) { return Rule.Rarity == EPRItemRarity::Uncommon; });
	const FPRRarityGenerationRule* Rare = DefaultRules.FindByPredicate([](const FPRRarityGenerationRule& Rule) { return Rule.Rarity == EPRItemRarity::Rare; });
	const FPRRarityGenerationRule* Prototype = DefaultRules.FindByPredicate([](const FPRRarityGenerationRule& Rule) { return Rule.Rarity == EPRItemRarity::Prototype; });
	TestTrue(TEXT("Common is 60 weight with no affixes"), Common && Common->Weight == 60.0f && Common->AffixCount == 0);
	TestTrue(TEXT("Uncommon is 25 weight with one affix"), Uncommon && Uncommon->Weight == 25.0f && Uncommon->AffixCount == 1);
	TestTrue(TEXT("Rare is 12 weight with two affixes"), Rare && Rare->Weight == 12.0f && Rare->AffixCount == 2);
	TestTrue(TEXT("Prototype is 3 weight with three affixes"), Prototype && Prototype->Weight == 3.0f && Prototype->AffixCount == 3);

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TestNotNull(TEXT("ProjectRift asset manager is initialized for affix catalog"), AssetManager);
	TArray<UPRAffixDefinitionDataAsset*> Catalog;
	TestTrue(TEXT("Affix catalog loads and validates"), AssetManager && AssetManager->LoadAffixCatalog(Catalog));
	TestEqual(TEXT("The first release contains exactly twelve affix definitions"), Catalog.Num(), 12);
	TSet<FName> ExpectedAffixIds = {
		TEXT("Affix.Vital.Flat"), TEXT("Affix.Vital.Percent"),
		TEXT("Affix.Shield.Flat"), TEXT("Affix.Shield.Percent"),
		TEXT("Affix.Energy.Flat"), TEXT("Affix.Energy.Percent"),
		TEXT("Affix.Attack.Flat"), TEXT("Affix.Attack.Percent"),
		TEXT("Affix.Mobility.Flat"), TEXT("Affix.Mobility.Percent"),
		TEXT("Affix.Healing.Flat"), TEXT("Affix.Pollution.Flat")
	};
	for (const UPRAffixDefinitionDataAsset* Definition : Catalog)
	{
		FString Diagnostic;
		TestTrue(FString::Printf(TEXT("Catalog affix %s validates"), *GetNameSafe(Definition)), Definition && Definition->ValidateDefinition(Diagnostic));
		if (Definition)
		{
			ExpectedAffixIds.Remove(Definition->AffixId);
		}
	}
	TestTrue(TEXT("Catalog contains every planned affix ID"), ExpectedAffixIds.IsEmpty());

	return true;
}

#endif
