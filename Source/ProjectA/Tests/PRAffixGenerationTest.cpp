#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"

#include "Abilities/PRAffixGameplayEffects.h"
#include "Core/PRGameplayTags.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PRAffixGenerationLibrary.h"
#include "Items/PREquipmentDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRAffixGenerationTest,
	"ProjectRift.Items.Affixes.Generation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UPRAffixDefinitionDataAsset* MakeAffix(
	const FName Id,
	const EPRAffixAttribute Attribute,
	const EPRAffixModifierType ModifierType,
	const EPREquipmentSlot Slot,
	const TSubclassOf<UGameplayEffect> EffectClass)
{
	UPRAffixDefinitionDataAsset* Affix = NewObject<UPRAffixDefinitionDataAsset>(GetTransientPackage());
	Affix->AffixId = Id;
	Affix->DisplayName = FText::FromName(Id);
	Affix->Description = FText::FromString(TEXT("Deterministic affix test definition."));
	Affix->AllowedSlots = { Slot };
	Affix->Attribute = Attribute;
	Affix->ModifierType = ModifierType;
	Affix->MinMagnitude = ModifierType == EPRAffixModifierType::Percentage ? 0.05f : 5.0f;
	Affix->MaxMagnitude = ModifierType == EPRAffixModifierType::Percentage ? 0.10f : 10.0f;
	Affix->Weight = 1.0f;
	Affix->ModifierEffectClass = EffectClass;
	return Affix;
}

TArray<FPRRarityGenerationRule> MakePrototypeRules()
{
	TArray<FPRRarityGenerationRule> Rules = UPRAffixGenerationLibrary::MakeDefaultRarityRules();
	for (FPRRarityGenerationRule& Rule : Rules)
	{
		Rule.Weight = Rule.Rarity == EPRItemRarity::Prototype ? 1000.0f : 0.001f;
	}
	return Rules;
}
}

bool FPRAffixGenerationTest::RunTest(const FString& Parameters)
{
	UPREquipmentDataAsset* Weapon = NewObject<UPREquipmentDataAsset>(GetTransientPackage());
	Weapon->ItemId = TEXT("Test.Weapon");
	Weapon->EquipmentSlot = EPREquipmentSlot::Weapon;

	UPRAffixDefinitionDataAsset* AttackFlat = MakeAffix(
		TEXT("Affix.Attack.Flat"), EPRAffixAttribute::AttackPower, EPRAffixModifierType::Additive,
		EPREquipmentSlot::Weapon, UPRAffixAttackPowerAdditiveGameplayEffect::StaticClass());
	UPRAffixDefinitionDataAsset* AttackPercent = MakeAffix(
		TEXT("Affix.Attack.Percent"), EPRAffixAttribute::AttackPower, EPRAffixModifierType::Percentage,
		EPREquipmentSlot::Weapon, UPRAffixAttackPowerPercentageGameplayEffect::StaticClass());
	AttackFlat->MutuallyExclusiveTags.AddTag(ProjectRiftGameplayTags::Data_Damage);
	AttackPercent->MutuallyExclusiveTags.AddTag(ProjectRiftGameplayTags::Data_Damage);
	UPRAffixDefinitionDataAsset* Energy = MakeAffix(
		TEXT("Affix.Energy.Flat"), EPRAffixAttribute::MaxEnergy, EPRAffixModifierType::Additive,
		EPREquipmentSlot::Weapon, UPRAffixMaxEnergyAdditiveGameplayEffect::StaticClass());
	UPRAffixDefinitionDataAsset* Move = MakeAffix(
		TEXT("Affix.Move.Flat"), EPRAffixAttribute::MoveSpeed, EPRAffixModifierType::Additive,
		EPREquipmentSlot::Weapon, UPRAffixMoveSpeedAdditiveGameplayEffect::StaticClass());
	TArray<UPRAffixDefinitionDataAsset*> Definitions = { Move, AttackPercent, Energy, AttackFlat };

	const TArray<FPRRarityGenerationRule> Rules = MakePrototypeRules();
	const FPRAffixGenerationResult First = UPRAffixGenerationLibrary::GenerateEquipmentInstance(Weapon, 1701, Definitions, Rules);
	const FPRAffixGenerationResult Second = UPRAffixGenerationLibrary::GenerateEquipmentInstance(Weapon, 1701, Definitions, Rules);
	TestTrue(TEXT("Seeded equipment generation succeeds"), First.bSuccess && Second.bSuccess);
	TestEqual(TEXT("Prototype rule produces three affixes"), First.Item.RolledAffixes.Num(), 3);
	TestTrue(TEXT("Same seed and definitions preserve every final roll"), First.Item.HasEquivalentStackingState(Second.Item));
	TestEqual(TEXT("Seed is persisted on generated item"), First.Item.LootSeed, 1701);
	TestEqual(TEXT("Generation version is persisted"), First.Item.AffixGenerationVersion, UPRAffixGenerationLibrary::CurrentGenerationVersion);

	Algo::Reverse(Definitions);
	const FPRAffixGenerationResult Reordered = UPRAffixGenerationLibrary::GenerateEquipmentInstance(Weapon, 1701, Definitions, Rules);
	TestTrue(TEXT("Candidate array order does not change deterministic result"), Reordered.bSuccess && First.Item.HasEquivalentStackingState(Reordered.Item));

	const bool bContainsFlatAttack = First.Item.Affixes.Contains(AttackFlat->AffixId);
	const bool bContainsPercentageAttack = First.Item.Affixes.Contains(AttackPercent->AffixId);
	TestFalse(TEXT("Mutually exclusive attack affixes never co-occur"), bContainsFlatAttack && bContainsPercentageAttack);

	UPRAffixDefinitionDataAsset* ArmorOnly = MakeAffix(
		TEXT("Affix.ArmorOnly"), EPRAffixAttribute::MaxHealth, EPRAffixModifierType::Additive,
		EPREquipmentSlot::Armor, UPRAffixMaxHealthAdditiveGameplayEffect::StaticClass());
	const FPRAffixGenerationResult InvalidSlot = UPRAffixGenerationLibrary::GenerateEquipmentInstance(Weapon, 1701, { ArmorOnly }, Rules);
	TestFalse(TEXT("Illegal slots fail closed instead of producing an invalid affix"), InvalidSlot.bSuccess);

	return true;
}

#endif
