#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "Tests/PRProjectSettingsTestUtils.h"
#include "UObject/StructOnScope.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRExtractedResourceRulesTest, "ProjectRift.Rift.ExtractedResourceRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRItemInstance MakeResourceRulesTestItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}

UPRItemDataAsset* MakeResourceRulesItemData(const FName ItemId, const EPRItemType ItemType)
{
	UPRItemDataAsset* ItemData = NewObject<UPRItemDataAsset>(GetTransientPackage());
	if (ItemData)
	{
		ItemData->ItemId = ItemId;
		ItemData->ItemType = ItemType;
		ItemData->MaxStackCount = 99;
	}

	return ItemData;
}

struct FResourceRulesTestPlayer
{
	TObjectPtr<APlayerController> Controller;
	TObjectPtr<APRPlayerState> PlayerState;
	TObjectPtr<ADefaultPawn> Pawn;
	TObjectPtr<UPRInventoryComponent> Inventory;
};

FResourceRulesTestPlayer SpawnResourceRulesTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
{
	FResourceRulesTestPlayer Result;
	if (!World)
	{
		Test.AddError(TEXT("Cannot spawn resource rules player without a world"));
		return Result;
	}

	Result.Controller = World->SpawnActor<APlayerController>();
	Result.PlayerState = World->SpawnActor<APRPlayerState>();
	Result.Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Resource rules controller spawned"), Result.Controller.Get());
	Test.TestNotNull(TEXT("Resource rules player state spawned"), Result.PlayerState.Get());
	Test.TestNotNull(TEXT("Resource rules pawn spawned"), Result.Pawn.Get());

	if (Result.Controller && Result.PlayerState)
	{
		Result.Controller->SetPlayerState(Result.PlayerState);
	}

	if (Result.Controller && Result.Pawn)
	{
		Result.Controller->Possess(Result.Pawn);
	}

	Result.Inventory = Result.PlayerState ? Result.PlayerState->GetInventoryComponent() : nullptr;
	Test.TestNotNull(TEXT("Resource rules player owns inventory"), Result.Inventory.Get());

	return Result;
}

bool CallResourceRulesBoolFunctionNoParams(UObject* Target, const FName FunctionName, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}
}

bool FPRExtractedResourceRulesTest::RunTest(const FString& Parameters)
{
	ProjectRiftTests::FScopedProjectSettingsOverride SettingsOverride;
	SettingsOverride->FailedResourceRetentionRate = 0.25f;

	UClass* PlayerStateClass = APRPlayerState::StaticClass();
	TestNotNull(TEXT("PlayerState exposes GetShipResourceCount"), PlayerStateClass->FindFunctionByName(TEXT("GetShipResourceCount")));
	TestNotNull(TEXT("PlayerState exposes GetShipResources"), PlayerStateClass->FindFunctionByName(TEXT("GetShipResources")));
	TestNotNull(TEXT("PlayerState exposes AddShipResource"), PlayerStateClass->FindFunctionByName(TEXT("AddShipResource")));
	TestNotNull(TEXT("PlayerState exposes ResetShipResources"), PlayerStateClass->FindFunctionByName(TEXT("ResetShipResources")));
	TestNotNull(TEXT("PlayerState exposes BuildShipResourceSummary"), PlayerStateClass->FindFunctionByName(TEXT("BuildShipResourceSummary")));

	UClass* RiftGameModeClass = APRRiftGameMode::StaticClass();
	TestNotNull(TEXT("Rift GameMode exposes CalculateRetainedResourceCount"), RiftGameModeClass->FindFunctionByName(TEXT("CalculateRetainedResourceCount")));
	TestNotNull(TEXT("Rift GameMode exposes IsExtractableResourceItem"), RiftGameModeClass->FindFunctionByName(TEXT("IsExtractableResourceItem")));
	TestNotNull(TEXT("Rift GameMode exposes RequestRiftFailure"), RiftGameModeClass->FindFunctionByName(TEXT("RequestRiftFailure")));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Resource rules test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Resource rules test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>();
	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Resource rules test world owns APRRiftGameMode"), RiftGameMode);
	TestNotNull(TEXT("Resource rules test world owns APRRiftGameState"), RiftGameState);
	if (!RiftGameMode || !RiftGameState)
	{
		return false;
	}

	RiftGameMode->SetReturnToLobbyServerTravelEnabled(false);

	UPRItemDataAsset* MaterialData = MakeResourceRulesItemData(TEXT("EnergyCrystal"), EPRItemType::Material);
	UPRItemDataAsset* EquipmentData = MakeResourceRulesItemData(TEXT("TestRifle"), EPRItemType::Equipment);
	UPRItemDataAsset* ConsumableData = MakeResourceRulesItemData(TEXT("HealthInjector"), EPRItemType::Consumable);
	UPRItemDataAsset* QuestData = MakeResourceRulesItemData(TEXT("QuestCore"), EPRItemType::QuestItem);
	TestNotNull(TEXT("Can create material item data"), MaterialData);
	TestNotNull(TEXT("Can create equipment item data"), EquipmentData);
	TestNotNull(TEXT("Can create consumable item data"), ConsumableData);
	TestNotNull(TEXT("Can create quest item data"), QuestData);

	FResourceRulesTestPlayer SuccessPlayer = SpawnResourceRulesTestPlayer(*this, World, FVector::ZeroVector);
	if (!SuccessPlayer.Controller || !SuccessPlayer.PlayerState || !SuccessPlayer.Inventory)
	{
		return false;
	}

	SuccessPlayer.Inventory->SetItemDataAssets({ MaterialData, EquipmentData, ConsumableData, QuestData });
	TestTrue(TEXT("Can seed success material stack"), SuccessPlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("EnergyCrystal"), 5)));
	TestTrue(TEXT("Can seed success equipment item"), SuccessPlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("TestRifle"), 1)));
	TestTrue(TEXT("Can seed success consumable stack"), SuccessPlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("HealthInjector"), 2)));
	TestTrue(TEXT("Can seed success quest stack"), SuccessPlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("QuestCore"), 3)));

	if (!RiftGameState->PlayerArray.Contains(SuccessPlayer.PlayerState))
	{
		RiftGameState->AddPlayerState(SuccessPlayer.PlayerState);
	}

	RiftGameState->SetAlivePlayerCount(1);
	RiftGameState->SetExtractionAvailable(true);
	TestTrue(TEXT("Success player can be registered as extracted"), RiftGameMode->RegisterPlayerExtracted(SuccessPlayer.Controller));
	TestEqual(TEXT("Successful extraction converts all material resources"), SuccessPlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 5);
	TestEqual(TEXT("Successful extraction removes converted material from inventory"), SuccessPlayer.Inventory->GetItemCount(TEXT("EnergyCrystal")), 0);
	TestEqual(TEXT("Equipment is not converted or deleted"), SuccessPlayer.Inventory->GetItemCount(TEXT("TestRifle")), 1);
	TestEqual(TEXT("Consumables are not converted or deleted"), SuccessPlayer.Inventory->GetItemCount(TEXT("HealthInjector")), 2);
	TestEqual(TEXT("Quest items are not converted by generic extraction rules"), SuccessPlayer.Inventory->GetItemCount(TEXT("QuestCore")), 3);
	TestTrue(TEXT("Lobby resource summary mentions converted resource"), SuccessPlayer.PlayerState->BuildShipResourceSummary().Contains(TEXT("EnergyCrystal x5")));

	APRPlayerState* CopiedTravelPlayerState = World->SpawnActor<APRPlayerState>();
	TestNotNull(TEXT("Can create copied travel player state"), CopiedTravelPlayerState);
	if (CopiedTravelPlayerState)
	{
		SuccessPlayer.PlayerState->CopyProperties(CopiedTravelPlayerState);
		TestEqual(TEXT("Ship resources copy across travel player state replacement"), CopiedTravelPlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 5);
	}

	const FPRRiftSettlementData SuccessSettlement = RiftGameState->GetSettlementData();
	TestEqual(TEXT("Settlement records successful extracted resources"), SuccessSettlement.ExtractedResourceCount, 5);
	TestEqual(TEXT("Settlement records no successful resource loss"), SuccessSettlement.LostResourceCount, 0);
	const FGuid SuccessfulRunId = SuccessSettlement.RunId;
	const FGuid SuccessfulSettlementId = SuccessSettlement.SettlementId;
	TestTrue(TEXT("Successful settlement has a run id"), SuccessfulRunId.IsValid());
	TestTrue(TEXT("Successful settlement has a settlement id"), SuccessfulSettlementId.IsValid());
	TestFalse(TEXT("Successful settlement has a mission id"), SuccessSettlement.MissionId.IsNone());

	RiftGameMode->FinalizeRiftSettlement(EPRRiftMissionResult::Success);
	const FPRRiftSettlementData DuplicateSuccessSettlement = RiftGameState->GetSettlementData();
	TestEqual(TEXT("Duplicate settlement keeps the same run id"), DuplicateSuccessSettlement.RunId, SuccessfulRunId);
	TestEqual(TEXT("Duplicate settlement keeps the same settlement id"), DuplicateSuccessSettlement.SettlementId, SuccessfulSettlementId);
	TestEqual(TEXT("Duplicate settlement does not grant resources twice"), SuccessPlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 5);

	FResourceRulesTestPlayer FailurePlayer = SpawnResourceRulesTestPlayer(*this, World, FVector(200.0f, 0.0f, 0.0f));
	if (!FailurePlayer.PlayerState || !FailurePlayer.Inventory)
	{
		return false;
	}

	FailurePlayer.Inventory->SetItemDataAssets({ MaterialData, EquipmentData, ConsumableData, QuestData });
	TestTrue(TEXT("Can seed failed material stack"), FailurePlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("EnergyCrystal"), 5)));
	TestTrue(TEXT("Can seed second failed material stack"), FailurePlayer.Inventory->AddItem(MakeResourceRulesTestItem(TEXT("CommonChip"), 4)));

	UPRItemDataAsset* CommonChipData = MakeResourceRulesItemData(TEXT("CommonChip"), EPRItemType::Material);
	FailurePlayer.Inventory->SetItemDataAssets({ MaterialData, EquipmentData, ConsumableData, QuestData, CommonChipData });

	TestTrue(TEXT("Can restart mission before testing failure entry"), RiftGameMode->StartRiftMission());
	RiftGameMode->SetReturnToLobbyServerTravelEnabled(false);
	if (!RiftGameState->PlayerArray.Contains(FailurePlayer.PlayerState))
	{
		RiftGameState->AddPlayerState(FailurePlayer.PlayerState);
	}

	bool bFailureRequested = false;
	TestTrue(
		TEXT("Can request rift failure through the public failure entry"),
		CallResourceRulesBoolFunctionNoParams(RiftGameMode, TEXT("RequestRiftFailure"), bFailureRequested));
	TestTrue(TEXT("Rift failure entry accepts the first failure request"), bFailureRequested);
	TestEqual(TEXT("Failed extraction uses configured retention for odd material count"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 1);
	TestEqual(TEXT("Failed extraction uses configured retention for even material count"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("CommonChip")), 1);
	TestEqual(TEXT("Failed extraction removes resolved material from inventory"), FailurePlayer.Inventory->GetItemCount(TEXT("EnergyCrystal")), 0);
	TestEqual(TEXT("Failed extraction removes second material from inventory"), FailurePlayer.Inventory->GetItemCount(TEXT("CommonChip")), 0);

	const FPRRiftSettlementData FailedSettlement = RiftGameState->GetSettlementData();
	TestEqual(TEXT("Failed settlement records failure result"), FailedSettlement.Result, EPRRiftMissionResult::Failed);
	TestEqual(TEXT("Failed settlement records retained resources"), FailedSettlement.ExtractedResourceCount, 2);
	TestEqual(TEXT("Failed settlement records lost resources"), FailedSettlement.LostResourceCount, 7);
	TestTrue(TEXT("Failure settlement requests return travel"), RiftGameMode->IsReturnToLobbyTravelPending());
	const int32 EnergyAfterFailure = FailurePlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal"));
	const int32 ChipAfterFailure = FailurePlayer.PlayerState->GetShipResourceCount(TEXT("CommonChip"));
	TestFalse(TEXT("Duplicate failure request is rejected"), RiftGameMode->RequestRiftFailure());
	TestEqual(TEXT("Duplicate failure does not grant EnergyCrystal twice"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), EnergyAfterFailure);
	TestEqual(TEXT("Duplicate failure does not grant CommonChip twice"), FailurePlayer.PlayerState->GetShipResourceCount(TEXT("CommonChip")), ChipAfterFailure);

	return true;
}

#endif
