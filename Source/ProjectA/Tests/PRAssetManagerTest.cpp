#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GeneralProjectSettings.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Misc/ConfigCacheIni.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Ship/PRShipRepairDataAsset.h"

namespace
{
struct FPRAssetManagerAsyncTestState
{
	double StartSeconds = 0.0;
	bool bItemCallbackCalled = false;
	bool bLootCallbackCalled = false;
	UPRItemDataAsset* Item = nullptr;
	UPRLootTableDataAsset* LootTable = nullptr;
	TSharedPtr<FStreamableHandle> ItemHandle;
	TSharedPtr<FStreamableHandle> LootHandle;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FVerifyPRAssetManagerAsyncLoads,
	TSharedRef<FPRAssetManagerAsyncTestState>,
	State,
	FAutomationTestBase*,
	Test);

bool FVerifyPRAssetManagerAsyncLoads::Update()
{
	const bool bCompleted = State->bItemCallbackCalled && State->bLootCallbackCalled;
	if (!bCompleted && FPlatformTime::Seconds() - State->StartSeconds < 10.0)
	{
		return false;
	}

	Test->TestTrue(TEXT("Item async callback executes"), State->bItemCallbackCalled);
	Test->TestEqual(
		TEXT("Async item has expected ItemId"),
		State->Item ? State->Item->ItemId : NAME_None,
		FName(TEXT("EnergyCrystal")));
	Test->TestTrue(TEXT("Loot table async callback executes"), State->bLootCallbackCalled);
	Test->TestEqual(
		TEXT("Async loot table has expected primary ID"),
		State->LootTable ? State->LootTable->GetPrimaryAssetId().ToString() : FString(),
		FString(TEXT("ProjectRiftLootTable:DA_TestLootTable")));
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRAssetManagerSyncTest,
	"ProjectRift.Assets.Manager.Sync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRAssetManagerSyncTest::RunTest(const FString& Parameters)
{
	FString AssetManagerClassName;
	GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("AssetManagerClassName"), AssetManagerClassName, GEngineIni);
	TestEqual(
		TEXT("Project config installs UPRAssetManager"),
		AssetManagerClassName,
		FString(TEXT("/Script/ProjectA.PRAssetManager")));

	TArray<FString> PrimaryAssetTypesToScan;
	GConfig->GetArray(
		TEXT("/Script/Engine.AssetManagerSettings"),
		TEXT("PrimaryAssetTypesToScan"),
		PrimaryAssetTypesToScan,
		GGameIni);

	const auto HasScanRule = [&PrimaryAssetTypesToScan](
		const TCHAR* AssetType,
		const TCHAR* AssetBaseClass,
		const TCHAR* Directory)
	{
		return PrimaryAssetTypesToScan.ContainsByPredicate(
			[AssetType, AssetBaseClass, Directory](const FString& Entry)
			{
				return Entry.Contains(FString::Printf(TEXT("PrimaryAssetType=\"%s\""), AssetType))
					&& Entry.Contains(FString::Printf(TEXT("AssetBaseClass=\"%s\""), AssetBaseClass))
					&& Entry.Contains(FString::Printf(TEXT("Path=\"%s\""), Directory))
					&& Entry.Contains(TEXT("CookRule=AlwaysCook"));
			});
	};

	TestTrue(
		TEXT("AssetManager scans ProjectRiftItem as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftItem"), TEXT("/Script/ProjectA.PRItemDataAsset"), TEXT("/Game/ProjectRift/Items")));
	TestTrue(
		TEXT("AssetManager scans ProjectRiftLootTable as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftLootTable"), TEXT("/Script/ProjectA.PRLootTableDataAsset"), TEXT("/Game/ProjectRift/Items")));
	TestTrue(
		TEXT("AssetManager scans ProjectRiftMission as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftMission"), TEXT("/Script/ProjectA.PRMissionProgressionDataAsset"), TEXT("/Game/ProjectRift/Missions")));
	TestTrue(
		TEXT("AssetManager scans ProjectRiftShipRepair as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftShipRepair"), TEXT("/Script/ProjectA.PRShipRepairDataAsset"), TEXT("/Game/ProjectRift/ShipRepairs")));

	TArray<FString> DirectoriesToAlwaysCook;
	GConfig->GetArray(
		TEXT("/Script/UnrealEd.ProjectPackagingSettings"),
		TEXT("DirectoriesToAlwaysCook"),
		DirectoriesToAlwaysCook,
		GGameIni);
	TestFalse(
		TEXT("Legacy ProjectRift Items AlwaysCook directory is removed"),
		DirectoriesToAlwaysCook.ContainsByPredicate([](const FString& Entry)
		{
			return Entry.Contains(TEXT("/Game/ProjectRift/Items"));
		}));

	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	TestNotNull(TEXT("General project settings exist"), ProjectSettings);
	TestEqual(
		TEXT("Project version is v0.5.4"),
		ProjectSettings ? ProjectSettings->ProjectVersion : FString(),
		FString(TEXT("0.5.4")));

	TestTrue(TEXT("Global manager is UPRAssetManager"), UAssetManager::Get().IsA<UPRAssetManager>());
	TestEqual(
		TEXT("HealthInjector item ID is canonical"),
		UPRAssetManager::MakeItemPrimaryAssetId(TEXT("HealthInjector")).ToString(),
		FString(TEXT("ProjectRiftItem:HealthInjector")));
	TestEqual(
		TEXT("Test loot table ID is canonical"),
		UPRAssetManager::MakeLootTablePrimaryAssetId(TEXT("DA_TestLootTable")).ToString(),
		FString(TEXT("ProjectRiftLootTable:DA_TestLootTable")));
	TestEqual(
		TEXT("Test mission ID is canonical"),
		UPRAssetManager::MakeMissionPrimaryAssetId(TEXT("Mission.Rift.Test.Hold")).ToString(),
		FString(TEXT("ProjectRiftMission:Mission.Rift.Test.Hold")));
	TestEqual(
		TEXT("Engine repair ID is canonical"),
		UPRAssetManager::MakeShipRepairPrimaryAssetId(TEXT("Repair.Ship.Engine.Stage1")).ToString(),
		FString(TEXT("ProjectRiftShipRepair:Repair.Ship.Engine.Stage1")));
	TestFalse(
		TEXT("Empty item ID is invalid"),
		UPRAssetManager::MakeItemPrimaryAssetId(NAME_None).IsValid());
	TestFalse(
		TEXT("Empty loot table ID is invalid"),
		UPRAssetManager::MakeLootTablePrimaryAssetId(NAME_None).IsValid());
	TestFalse(
		TEXT("Empty mission ID is invalid"),
		UPRAssetManager::MakeMissionPrimaryAssetId(NAME_None).IsValid());
	TestFalse(
		TEXT("Empty ship repair ID is invalid"),
		UPRAssetManager::MakeShipRepairPrimaryAssetId(NAME_None).IsValid());

	UPRAssetManager* Manager = UPRAssetManager::Get();
	TestNotNull(TEXT("Typed ProjectRift AssetManager exists"), Manager);
	if (!Manager)
	{
		return false;
	}

	TArray<FPrimaryAssetId> ItemIds;
	TArray<FPrimaryAssetId> LootTableIds;
	TArray<FPrimaryAssetId> MissionIds;
	TArray<FPrimaryAssetId> ShipRepairIds;
	Manager->GetPrimaryAssetIdList(UPRItemDataAsset::ItemPrimaryAssetType, ItemIds);
	Manager->GetPrimaryAssetIdList(UPRLootTableDataAsset::LootTablePrimaryAssetType, LootTableIds);
	Manager->GetPrimaryAssetIdList(UPRMissionProgressionDataAsset::MissionPrimaryAssetType, MissionIds);
	Manager->GetPrimaryAssetIdList(UPRShipRepairDataAsset::ShipRepairPrimaryAssetType, ShipRepairIds);
	TestEqual(TEXT("Four ProjectRift item assets are registered"), ItemIds.Num(), 4);
	TestEqual(TEXT("One ProjectRift loot table is registered"), LootTableIds.Num(), 1);
	TestEqual(TEXT("One ProjectRift mission is registered"), MissionIds.Num(), 1);
	TestEqual(TEXT("One ProjectRift ship repair is registered"), ShipRepairIds.Num(), 1);

	UPRItemDataAsset* HealthInjector = Manager->LoadItemDataSync(TEXT("HealthInjector"));
	TestNotNull(TEXT("HealthInjector loads by PrimaryAssetId"), HealthInjector);
	TestEqual(
		TEXT("Loaded HealthInjector has the canonical primary ID"),
		HealthInjector ? HealthInjector->GetPrimaryAssetId().ToString() : FString(),
		FString(TEXT("ProjectRiftItem:HealthInjector")));

	UPRLootTableDataAsset* TestLootTable = Manager->LoadLootTableSync(TEXT("DA_TestLootTable"));
	TestNotNull(TEXT("Test loot table loads by PrimaryAssetId"), TestLootTable);
	TestEqual(
		TEXT("Loaded loot table has the canonical primary ID"),
		TestLootTable ? TestLootTable->GetPrimaryAssetId().ToString() : FString(),
		FString(TEXT("ProjectRiftLootTable:DA_TestLootTable")));

	UPRMissionProgressionDataAsset* TestMission = Manager->LoadMissionSync(TEXT("Mission.Rift.Test.Hold"));
	TestNotNull(TEXT("Test mission loads by PrimaryAssetId"), TestMission);
	TestTrue(TEXT("Loaded test mission contract is valid"), TestMission && TestMission->IsContractValid());

	UPRShipRepairDataAsset* EngineRepair = Manager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage1"));
	TestNotNull(TEXT("Engine repair loads by PrimaryAssetId"), EngineRepair);
	TestTrue(TEXT("Loaded engine repair contract is valid"), EngineRepair && EngineRepair->IsContractValid());
	TestEqual(TEXT("Engine repair targets the canonical module"), EngineRepair ? EngineRepair->ModuleId : NAME_None, FName(TEXT("Ship.Module.Engine")));
	TestEqual(TEXT("Engine repair targets level one"), EngineRepair ? EngineRepair->TargetLevel : INDEX_NONE, 1);
	TestEqual(TEXT("Engine repair has one resource cost"), EngineRepair ? EngineRepair->ResourceCosts.Num() : INDEX_NONE, 1);
	TestEqual(TEXT("Engine repair costs ten EnergyCrystal"), EngineRepair ? EngineRepair->ResourceCosts[0].Count : INDEX_NONE, 10);
	TestTrue(TEXT("Engine repair requires the prologue completion node"), EngineRepair && EngineRepair->RequiredCompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftTestHold")));
	TestTrue(TEXT("Engine repair unlocks Chapter One"), EngineRepair && EngineRepair->UnlockedChapterIdsOnCompletion.Contains(TEXT("Chapter.One")));
	TestEqual(TEXT("Engine repair advances to Chapter One"), EngineRepair ? EngineRepair->NextChapterId : NAME_None, FName(TEXT("Chapter.One")));
	TArray<UPRShipRepairDataAsset*> ShipRepairCatalog;
	TestTrue(TEXT("AssetManager builds a valid ship repair catalog"), Manager->LoadShipRepairCatalog(ShipRepairCatalog));
	TestEqual(TEXT("Loaded ship repair catalog contains one contract"), ShipRepairCatalog.Num(), 1);

	AddExpectedError(
		TEXT("Primary asset is not registered"),
		EAutomationExpectedErrorFlags::Contains,
		2);
	TestNull(TEXT("Unknown item returns null"), Manager->LoadItemDataSync(TEXT("MissingItem")));
	TestNull(TEXT("Loot table name cannot load as an item"), Manager->LoadItemDataSync(TEXT("DA_TestLootTable")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRAssetManagerAsyncTest,
	"ProjectRift.Assets.Manager.Async",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRAssetManagerAsyncTest::RunTest(const FString& Parameters)
{
	UPRAssetManager* Manager = UPRAssetManager::Get();
	TestNotNull(TEXT("Typed ProjectRift AssetManager exists for async loading"), Manager);
	if (!Manager)
	{
		return false;
	}

	TSharedRef<FPRAssetManagerAsyncTestState> State = MakeShared<FPRAssetManagerAsyncTestState>();
	State->StartSeconds = FPlatformTime::Seconds();
	State->ItemHandle = Manager->LoadItemDataAsync(
		TEXT("EnergyCrystal"),
		FPRItemDataLoadComplete::CreateLambda(
			[State](UPRItemDataAsset* LoadedItem)
			{
				State->bItemCallbackCalled = true;
				State->Item = LoadedItem;
			}));
	TestTrue(TEXT("Known item async request returns a handle"), State->ItemHandle.IsValid());

	State->LootHandle = Manager->LoadLootTableAsync(
		TEXT("DA_TestLootTable"),
		FPRLootTableLoadComplete::CreateLambda(
			[State](UPRLootTableDataAsset* LoadedLootTable)
			{
				State->bLootCallbackCalled = true;
				State->LootTable = LoadedLootTable;
			}));
	TestTrue(TEXT("Known loot table async request returns a handle"), State->LootHandle.IsValid());

	int32 InvalidCallbackCount = 0;
	UPRItemDataAsset* InvalidCallbackResult = NewObject<UPRItemDataAsset>(GetTransientPackage());
	TSharedPtr<FStreamableHandle> InvalidHandle = Manager->LoadItemDataAsync(
		NAME_None,
		FPRItemDataLoadComplete::CreateLambda(
			[&InvalidCallbackCount, &InvalidCallbackResult](UPRItemDataAsset* LoadedItem)
			{
				++InvalidCallbackCount;
				InvalidCallbackResult = LoadedItem;
			}));
	TestFalse(TEXT("Empty item async request returns no handle"), InvalidHandle.IsValid());
	TestEqual(TEXT("Empty item async callback executes once"), InvalidCallbackCount, 1);
	TestNull(TEXT("Empty item async callback receives null"), InvalidCallbackResult);

	ADD_LATENT_AUTOMATION_COMMAND(FVerifyPRAssetManagerAsyncLoads(State, this));

	return true;
}

#endif
