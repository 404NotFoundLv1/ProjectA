#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Items/PRRewardBudgetDataAsset.h"
#include "Items/PRRewardGenerationLibrary.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRPickupActor.h"
#include "Core/PRRiftGameMode.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Persistence/PRProfileSave.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRewardBudgetContractTest,
	"ProjectRift.Rewards.BudgetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRewardBudgetContractTest::RunTest(const FString& Parameters)
{
	UClass* BudgetClass = UPRRewardBudgetDataAsset::StaticClass();
	TestNotNull(TEXT("Reward budget is a reflected primary data asset"), BudgetClass);
	TestNotNull(TEXT("Reward budget exposes its source definitions"), FindFProperty<FArrayProperty>(BudgetClass, TEXT("SourceDefinitions")));
	TestNotNull(TEXT("Reward budget exposes its personal equipment table"), FindFProperty<FObjectPropertyBase>(BudgetClass, TEXT("PersonalEquipmentLootTable")));
	TestNotNull(TEXT("Reward budget exposes a per-player base budget"), FindFProperty<FNumericProperty>(BudgetClass, TEXT("BaseSuccessBudget")));
	TestNotNull(TEXT("Reward budget exposes a frozen-party scaling value"), FindFProperty<FNumericProperty>(BudgetClass, TEXT("AdditionalPlayerBudget")));
	TestNotNull(TEXT("Reward budget exposes its objective reward value"), FindFProperty<FNumericProperty>(BudgetClass, TEXT("ObjectiveBudget")));
	TestNotNull(TEXT("Reward budget exposes pity threshold"), FindFProperty<FNumericProperty>(BudgetClass, TEXT("RarePityThreshold")));

	TestEqual(
		TEXT("Reward budget uses its dedicated primary asset type"),
		UPRRewardBudgetDataAsset::RewardBudgetPrimaryAssetType.ToString(),
		FString(TEXT("ProjectRiftRewardBudget")));
	TestEqual(TEXT("Reward source type has shared enemy world drops"), static_cast<uint8>(EPRRewardSourceType::Enemy), static_cast<uint8>(0));
	TestEqual(TEXT("Reward source type has settlement rewards"), static_cast<uint8>(EPRRewardSourceType::MissionSettlement), static_cast<uint8>(1));
	TestEqual(TEXT("Distribution policy keeps shared world loot"), static_cast<uint8>(EPRLootDistributionPolicy::SharedWorld), static_cast<uint8>(0));
	TestEqual(TEXT("Distribution policy supports direct personal settlement"), static_cast<uint8>(EPRLootDistributionPolicy::PersonalSettlement), static_cast<uint8>(1));

	UScriptStruct* SourceContextStruct = FPRRewardSourceContext::StaticStruct();
	TestNotNull(TEXT("Reward source context is reflected"), SourceContextStruct);
	TestNotNull(TEXT("Source context records a source id"), FindFProperty<FProperty>(SourceContextStruct, TEXT("SourceId")));
	TestNotNull(TEXT("Source context records an owning recipient"), FindFProperty<FProperty>(SourceContextStruct, TEXT("RecipientProfileId")));
	TestNotNull(TEXT("Source context records deterministic seed"), FindFProperty<FProperty>(SourceContextStruct, TEXT("Seed")));

	UClass* GeneratorClass = UPRRewardGenerationLibrary::StaticClass();
	TestNotNull(TEXT("Reward generation exposes personal settlement generation"), GeneratorClass->FindFunctionByName(TEXT("GeneratePersonalSettlementReward")));
	TestNotNull(TEXT("Reward generation exposes deterministic sub-seed derivation"), GeneratorClass->FindFunctionByName(TEXT("DeriveSeed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRewardGenerationTest,
	"ProjectRift.Rewards.DeterministicGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRLootTableEntry MakePersonalRewardEntry(const FName ItemId, const EPRItemRarity Rarity, const float Weight)
{
	FPRLootTableEntry Entry;
	Entry.Item.ItemId = ItemId;
	Entry.Item.Count = 1;
	Entry.Item.Rarity = Rarity;
	Entry.Weight = Weight;
	return Entry;
}

FPRRewardSourceContext MakePersonalRewardSource(const int32 Seed)
{
	FPRRewardSourceContext Source;
	Source.SourceType = EPRRewardSourceType::MissionSettlement;
	Source.SourceId = TEXT("Mission.Prologue.RiftTest");
	Source.RecipientProfileId = FGuid(0x1A2B3C4Du, 0x5E6F7788u, 0x10293847u, 0x55667788u);
	Source.Seed = Seed;
	Source.Ordinal = 0;
	return Source;
}
}

bool FPRRewardGenerationTest::RunTest(const FString& Parameters)
{
	UPRLootTableDataAsset* PersonalTable = NewObject<UPRLootTableDataAsset>(GetTransientPackage());
	PersonalTable->Entries = {
		MakePersonalRewardEntry(TEXT("TestRifle"), EPRItemRarity::Common, 70.0f),
		MakePersonalRewardEntry(TEXT("TestArmor"), EPRItemRarity::Rare, 30.0f)
	};
	UPRRewardBudgetDataAsset* Budget = NewObject<UPRRewardBudgetDataAsset>(GetTransientPackage());
	Budget->PersonalEquipmentLootTable = PersonalTable;

	const FPRRewardSourceContext Source = MakePersonalRewardSource(1717);
	const FPRPersonalRewardGenerationResult Solo = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(Budget, Source, FPRLootProtectionState(), 1);
	const FPRPersonalRewardGenerationResult FourPlayers = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(Budget, Source, FPRLootProtectionState(), 4);
	TestTrue(TEXT("A valid successful recipient receives a reward result"), Solo.bSuccess);
	TestEqual(TEXT("Solo success allocates the 100 reward budget"), Solo.TotalBudget, 100);
	TestEqual(TEXT("Four-player frozen difficulty allocates 175 reward budget"), FourPlayers.TotalBudget, 175);
	TestEqual(TEXT("Solo has no leftover chip reward"), Solo.CommonChipCount, 0);
	TestEqual(TEXT("Four-player budget converts the 75 remainder to three chips"), FourPlayers.CommonChipCount, 3);
	TestEqual(TEXT("One equipment item consumes exactly 100 budget"), Solo.GrantedWarehouseItems.Num(), 1);

	FPRRewardRuntimeModifiers RiskAndOptionalModifiers;
	RiskAndOptionalModifiers.Multiplier = 1.5f;
	RiskAndOptionalModifiers.FlatBonusBudget = 25;
	const FPRPersonalRewardGenerationResult RiskAndOptional = UPRRewardGenerationLibrary::GeneratePersonalSettlementRewardWithModifiers(
		Budget, Source, FPRLootProtectionState(), 1, RiskAndOptionalModifiers);
	TestEqual(TEXT("Peak risk and completed optional budget compose deterministically"), RiskAndOptional.TotalBudget, 175);
	TestEqual(TEXT("Modified budget retains the existing chip conversion"), RiskAndOptional.CommonChipCount, 3);

	const FPRPersonalRewardGenerationResult Repeated = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(Budget, Source, FPRLootProtectionState(), 1);
	TestEqual(TEXT("Same source seed produces the same item id"), Repeated.GrantedWarehouseItems[0].ItemId, Solo.GrantedWarehouseItems[0].ItemId);
	TestTrue(TEXT("Same source seed produces equivalent generated item state"), Repeated.GrantedWarehouseItems[0].HasEquivalentStackingState(Solo.GrantedWarehouseItems[0]));

	FPRLootProtectionState PityState;
	PityState.RewardBudgetId = Budget->GetFName();
	PityState.ConsecutiveBelowRare = 3;
	const FPRPersonalRewardGenerationResult PityReward = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(Budget, Source, PityState, 1);
	TestTrue(TEXT("Fourth below-rare roll is promoted to Rare or better"), PityReward.GrantedWarehouseItems[0].Rarity >= EPRItemRarity::Rare);
	TestEqual(TEXT("Rare reward resets the pity counter"), PityReward.UpdatedProtectionState.ConsecutiveBelowRare, 0);

	FPRLootProtectionState RepeatState;
	RepeatState.RewardBudgetId = Budget->GetFName();
	RepeatState.LastItemId = Solo.GrantedWarehouseItems[0].ItemId;
	RepeatState.ConsecutiveSameItem = 2;
	const FPRPersonalRewardGenerationResult RepeatProtected = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(Budget, Source, RepeatState, 1);
	TestNotEqual(TEXT("Third identical item is replaced when a legal table alternative exists"), RepeatProtected.GrantedWarehouseItems[0].ItemId, RepeatState.LastItemId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRewardPersistenceContractTest,
	"ProjectRift.Rewards.PersistenceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRewardPersistenceContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Personal reward protection remains available after schema v9"), UPRProfileSave::LatestSaveVersion, 9);
	TestNotNull(TEXT("Profile snapshot persists protection per reward budget"), FindFProperty<FArrayProperty>(FPRProfileSnapshot::StaticStruct(), TEXT("LootProtectionStates")));

	UScriptStruct* ReceiptStruct = FPRPlayerSettlementReceipt::StaticStruct();
	TestNotNull(TEXT("Personal settlement receipt carries warehouse-only grants"), FindFProperty<FArrayProperty>(ReceiptStruct, TEXT("GrantedWarehouseItems")));
	TestNotNull(TEXT("Personal settlement receipt carries updated protection"), FindFProperty<FStructProperty>(ReceiptStruct, TEXT("UpdatedLootProtectionState")));
	TestNotNull(TEXT("Personal settlement receipt records audit entries"), FindFProperty<FArrayProperty>(ReceiptStruct, TEXT("RewardAuditEntries")));
	TestNotNull(TEXT("Personal settlement receipt records deterministic run seed"), FindFProperty<FNumericProperty>(ReceiptStruct, TEXT("RunSeed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRewardMissionRuntimeContractTest,
	"ProjectRift.Rewards.MissionRuntimeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRewardMissionRuntimeContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Loot tables explicitly declare shared versus personal distribution"), FindFProperty<FProperty>(UPRLootTableDataAsset::StaticClass(), TEXT("DistributionPolicy")));
	UClass* GameModeClass = APRRiftGameMode::StaticClass();
	TestNotNull(TEXT("Mission runtime exposes a deterministic run seed"), GameModeClass->FindFunctionByName(TEXT("GetCurrentRunSeed")));
	TestNotNull(TEXT("Mission runtime persists frozen eligible reward profiles"), FindFProperty<FArrayProperty>(GameModeClass, TEXT("EligibleRewardProfileIds")));
	TestNotNull(TEXT("Mission runtime can derive server-owned source seeds"), GameModeClass->FindFunctionByName(TEXT("AllocateRewardSeed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRSharedWorldLootContractTest,
	"ProjectRift.Rewards.SharedWorldLootContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSharedWorldLootContractTest::RunTest(const FString& Parameters)
{
	UPRLootTableDataAsset* Table = NewObject<UPRLootTableDataAsset>(GetTransientPackage());
	TestEqual(TEXT("Default enemy world loot remains shared"), Table->DistributionPolicy, EPRLootDistributionPolicy::SharedWorld);
	TestEqual(TEXT("Default shared table ends with CommonChip rather than equipment"), Table->Entries.Last().Item.ItemId, FName(TEXT("CommonChip")));
	TestFalse(TEXT("Shared world chip does not generate equipment affixes"), Table->Entries.Last().bGenerateEquipmentAffixes);
	TestNotNull(TEXT("World pickup carries a recorded authoritative reward source"), FindFProperty<FStructProperty>(APRPickupActor::StaticClass(), TEXT("RewardSource")));
	TestNotNull(TEXT("World pickup accepts a server-authored reward source"), APRPickupActor::StaticClass()->FindFunctionByName(TEXT("SetRewardSource")));
	return true;
}

#endif
