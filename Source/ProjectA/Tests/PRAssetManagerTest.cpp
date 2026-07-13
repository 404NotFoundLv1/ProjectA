#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GeneralProjectSettings.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Misc/ConfigCacheIni.h"

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
		const TCHAR* AssetBaseClass)
	{
		return PrimaryAssetTypesToScan.ContainsByPredicate(
			[AssetType, AssetBaseClass](const FString& Entry)
			{
				return Entry.Contains(FString::Printf(TEXT("PrimaryAssetType=\"%s\""), AssetType))
					&& Entry.Contains(FString::Printf(TEXT("AssetBaseClass=\"%s\""), AssetBaseClass))
					&& Entry.Contains(TEXT("Path=\"/Game/ProjectRift/Items\""))
					&& Entry.Contains(TEXT("CookRule=AlwaysCook"));
			});
	};

	TestTrue(
		TEXT("AssetManager scans ProjectRiftItem as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftItem"), TEXT("/Script/ProjectA.PRItemDataAsset")));
	TestTrue(
		TEXT("AssetManager scans ProjectRiftLootTable as AlwaysCook"),
		HasScanRule(TEXT("ProjectRiftLootTable"), TEXT("/Script/ProjectA.PRLootTableDataAsset")));

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
		TEXT("Project version is v0.5.2"),
		ProjectSettings ? ProjectSettings->ProjectVersion : FString(),
		FString(TEXT("0.5.2")));

	TestTrue(TEXT("Global manager is UPRAssetManager"), UAssetManager::Get().IsA<UPRAssetManager>());
	TestEqual(
		TEXT("HealthInjector item ID is canonical"),
		UPRAssetManager::MakeItemPrimaryAssetId(TEXT("HealthInjector")).ToString(),
		FString(TEXT("ProjectRiftItem:HealthInjector")));
	TestEqual(
		TEXT("Test loot table ID is canonical"),
		UPRAssetManager::MakeLootTablePrimaryAssetId(TEXT("DA_TestLootTable")).ToString(),
		FString(TEXT("ProjectRiftLootTable:DA_TestLootTable")));
	TestFalse(
		TEXT("Empty item ID is invalid"),
		UPRAssetManager::MakeItemPrimaryAssetId(NAME_None).IsValid());
	TestFalse(
		TEXT("Empty loot table ID is invalid"),
		UPRAssetManager::MakeLootTablePrimaryAssetId(NAME_None).IsValid());

	UPRAssetManager* Manager = UPRAssetManager::Get();
	TestNotNull(TEXT("Typed ProjectRift AssetManager exists"), Manager);
	if (!Manager)
	{
		return false;
	}

	TArray<FPrimaryAssetId> ItemIds;
	TArray<FPrimaryAssetId> LootTableIds;
	Manager->GetPrimaryAssetIdList(UPRItemDataAsset::ItemPrimaryAssetType, ItemIds);
	Manager->GetPrimaryAssetIdList(UPRLootTableDataAsset::LootTablePrimaryAssetType, LootTableIds);
	TestEqual(TEXT("Four ProjectRift item assets are registered"), ItemIds.Num(), 4);
	TestEqual(TEXT("One ProjectRift loot table is registered"), LootTableIds.Num(), 1);

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
