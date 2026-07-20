#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTransactionComponent.h"
#include "Items/PRWarehouseComponent.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationCommon.h"
#include "UI/PRInventoryViewModel.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRInventoryPresentationContractTest,
	"ProjectRift.Inventory.Presentation.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRInventoryPresentationContractTest::RunTest(const FString& Parameters)
{
	UClass* ViewModelClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRInventoryViewModel"));
	TestNotNull(TEXT("Inventory presentation exposes a reflected ViewModel"), ViewModelClass);

	UClass* WarehouseClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWarehouseComponent"));
	TestNotNull(TEXT("Player-owned warehouse exposes a reflected component"), WarehouseClass);

	const UClass* PlayerStateClass = APRPlayerState::StaticClass();
	const UFunction* WarehouseAccessor = PlayerStateClass->FindFunctionByName(TEXT("GetWarehouseComponent"));
	TestNotNull(TEXT("PlayerState exposes its warehouse component"), WarehouseAccessor);
	if (WarehouseAccessor && WarehouseClass)
	{
		const FObjectProperty* ReturnProperty = FindFProperty<FObjectProperty>(WarehouseAccessor, TEXT("ReturnValue"));
		TestTrue(
			TEXT("PlayerState warehouse accessor returns the warehouse component type"),
			ReturnProperty && ReturnProperty->PropertyClass->IsChildOf(WarehouseClass));
	}

	const UClass* ControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("PlayerController exposes its inventory presentation ViewModel"), ControllerClass->FindFunctionByName(TEXT("GetInventoryViewModel")));
	TestNotNull(TEXT("PlayerController exposes the inventory presentation rebind entry point"), ControllerClass->FindFunctionByName(TEXT("RebindInventoryPresentation")));
	TestNotNull(TEXT("PlayerController exposes an authoritative backpack-to-warehouse command"), ControllerClass->FindFunctionByName(TEXT("StoreInventoryInstanceInWarehouse")));
	TestNotNull(TEXT("PlayerController exposes an authoritative warehouse-to-backpack command"), ControllerClass->FindFunctionByName(TEXT("RetrieveWarehouseInstance")));

	const UEnum* IntentEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRItemTransactionIntent"));
	TestNotNull(TEXT("Item transaction intent enum is reflected"), IntentEnum);
	if (IntentEnum)
	{
		TestTrue(TEXT("Warehouse store transaction intent exists"), IntentEnum->GetIndexByName(TEXT("StoreInWarehouse")) != INDEX_NONE);
		TestTrue(TEXT("Warehouse retrieve transaction intent exists"), IntentEnum->GetIndexByName(TEXT("RetrieveFromWarehouse")) != INDEX_NONE);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRInventoryPresentationRuntimeTest,
	"ProjectRift.Inventory.Presentation.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRInventoryPresentationRuntimeTest::RunTest(const FString& Parameters)
{
	APRPlayerState* SourceState = NewObject<APRPlayerState>(GetTransientPackage());
	APRPlayerState* ReplacementState = NewObject<APRPlayerState>(GetTransientPackage());
	TestNotNull(TEXT("Can create source PlayerState"), SourceState);
	TestNotNull(TEXT("Can create replacement PlayerState"), ReplacementState);
	if (!SourceState || !ReplacementState)
	{
		return false;
	}

	FPRItemInstance LowLevel; LowLevel.InstanceGuid = FGuid::NewGuid(); LowLevel.ItemId = TEXT("PresentationLow"); LowLevel.Count = 1; LowLevel.Level = 1;
	FPRItemInstance HighLevel; HighLevel.InstanceGuid = FGuid::NewGuid(); HighLevel.ItemId = TEXT("PresentationHigh"); HighLevel.Count = 1; HighLevel.Level = 3;
	FPRItemInstance Stored; Stored.InstanceGuid = FGuid::NewGuid(); Stored.ItemId = TEXT("PresentationStored"); Stored.Count = 4;
	TestTrue(TEXT("Can seed authoritative backpack rows"), SourceState->GetInventoryComponent()->ReplaceInventoryItems({ LowLevel, HighLevel }));
	TestTrue(TEXT("Can seed authoritative warehouse rows"), SourceState->GetWarehouseComponent()->ReplaceWarehouseItems({ Stored }));

	UPRInventoryViewModel* ViewModel = NewObject<UPRInventoryViewModel>(GetTransientPackage());
	TestNotNull(TEXT("Can create inventory presentation ViewModel"), ViewModel);
	if (!ViewModel)
	{
		return false;
	}
	ViewModel->BindPlayerState(SourceState);
	const TArray<FPRInventoryPresentationRow> AuthorityRows = ViewModel->GetVisibleRows();
	TestEqual(TEXT("Backpack presentation derives both authoritative rows"), AuthorityRows.Num(), 2);
	ViewModel->SetSort(EPRInventorySortField::Level, false);
	const TArray<FPRInventoryPresentationRow> SortedRows = ViewModel->GetVisibleRows();
	TestEqual(TEXT("Sorting changes presentation order"), SortedRows.IsEmpty() ? NAME_None : SortedRows[0].Item.ItemId, FName(TEXT("PresentationHigh")));
	TestEqual(TEXT("Sorting does not mutate authoritative inventory order"), SourceState->GetInventoryComponent()->GetInventoryItems()[0].ItemId, FName(TEXT("PresentationLow")));
	ViewModel->SelectInstance(HighLevel.InstanceGuid);
	TestEqual(TEXT("Selection is stored by stable item identity"), ViewModel->GetSelectedInstanceGuid(), HighLevel.InstanceGuid);
	// FName fallback display names insert a space before the trailing capital, so this filter keeps only the selected row.
	ViewModel->SetFilter(EPRInventoryFilter::All, TEXT("High"));
	TestEqual(TEXT("Filtering retains the selected stable identity"), ViewModel->GetSelectedInstanceGuid(), HighLevel.InstanceGuid);
	ViewModel->SetFilter(EPRInventoryFilter::All, FString());
	ViewModel->SetSource(EPRInventoryPresentationSource::Warehouse);
	TestEqual(TEXT("Warehouse presentation derives warehouse rows"), ViewModel->GetVisibleRows().Num(), 1);
	TestEqual(TEXT("Warehouse presentation keeps the stored instance identity"), ViewModel->GetVisibleRows()[0].Item.InstanceGuid, Stored.InstanceGuid);

	SourceState->CopyProperties(ReplacementState);
	TestEqual(TEXT("Warehouse survives seamless PlayerState replacement"), ReplacementState->GetWarehouseComponent()->GetWarehouseItems().Num(), 1);
	TestEqual(TEXT("Warehouse item identity survives seamless PlayerState replacement"), ReplacementState->GetWarehouseComponent()->GetWarehouseItems()[0].InstanceGuid, Stored.InstanceGuid);

	UPRItemTransactionComponent* Transactions = SourceState->GetItemTransactionComponent();
	FPRItemTransactionRequest Request;
	Request.Header.TransactionId = FGuid::NewGuid();
	Request.Header.ExpectedRevision = Transactions->GetRevision();
	Request.Intent = EPRItemTransactionIntent::StoreInWarehouse;
	Request.InstanceGuid = LowLevel.InstanceGuid;
	Request.Count = 1;
	const FPRItemTransactionResult Rejected = Transactions->ExecuteAuthoritativeTransaction(Request);
	TestEqual(TEXT("Warehouse transfer outside an authoritative ship lobby is fail-closed"), Rejected.Status, EPRItemTransactionStatus::Unauthorized);
	const FPRItemTransactionResult Replay = Transactions->ExecuteAuthoritativeTransaction(Request);
	TestTrue(TEXT("Rejected warehouse transaction replays its cached result"), Replay.bWasReplay);
	TestEqual(TEXT("Rejected transfer does not remove the backpack item"), SourceState->GetInventoryComponent()->GetInventoryItems().Num(), 2);
	TestEqual(TEXT("Rejected transfer does not duplicate the warehouse item"), SourceState->GetWarehouseComponent()->GetWarehouseItems().Num(), 1);

	FTestWorldWrapper LobbyWorldWrapper;
	TestTrue(TEXT("Ship-lobby transaction world is created"), LobbyWorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* LobbyWorld = LobbyWorldWrapper.GetTestWorld();
	if (!LobbyWorld)
	{
		return false;
	}
	if (AWorldSettings* WorldSettings = LobbyWorld->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRShipLobbyGameMode::StaticClass();
	}
	TestTrue(TEXT("Ship-lobby transaction world begins play"), LobbyWorldWrapper.BeginPlayInTestWorld());
	if (LobbyWorldWrapper.HasFailed())
	{
		LobbyWorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	TestNotNull(TEXT("Warehouse transactions require the actual ship lobby GameMode"), LobbyWorld->GetAuthGameMode<APRShipLobbyGameMode>());

	APRPlayerState* LobbyPlayerState = LobbyWorld->SpawnActor<APRPlayerState>();
	TestNotNull(TEXT("Ship-lobby PlayerState is spawned"), LobbyPlayerState);
	if (!LobbyPlayerState)
	{
		return false;
	}
	FPRItemInstance Transferable;
	Transferable.InstanceGuid = FGuid::NewGuid();
	Transferable.ItemId = TEXT("WarehouseTransferable");
	Transferable.Count = 3;
	TestTrue(TEXT("Can seed a lobby backpack stack for warehouse transfer"), LobbyPlayerState->GetInventoryComponent()->ReplaceInventoryItems({ Transferable }));

	UPRItemTransactionComponent* LobbyTransactions = LobbyPlayerState->GetItemTransactionComponent();
	FPRItemTransactionRequest StoreRequest;
	StoreRequest.Header.TransactionId = FGuid::NewGuid();
	StoreRequest.Header.ExpectedRevision = LobbyTransactions->GetRevision();
	StoreRequest.Intent = EPRItemTransactionIntent::StoreInWarehouse;
	StoreRequest.InstanceGuid = Transferable.InstanceGuid;
	StoreRequest.Count = 2;
	const FPRItemTransactionResult StoredResult = LobbyTransactions->ExecuteAuthoritativeTransaction(StoreRequest);
	TestEqual(TEXT("Lobby store transfer succeeds authoritatively"), StoredResult.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Lobby store transfer applies the requested partial quantity"), StoredResult.AppliedCount, 2);
	TestEqual(TEXT("Lobby source retains the untransferred quantity"), LobbyPlayerState->GetInventoryComponent()->GetItemCount(Transferable.ItemId), 1);
	TestEqual(TEXT("Lobby warehouse receives the transferred quantity"), LobbyPlayerState->GetWarehouseComponent()->GetItemCount(Transferable.ItemId), 2);
	const FPRItemTransactionResult StoreReplay = LobbyTransactions->ExecuteAuthoritativeTransaction(StoreRequest);
	TestTrue(TEXT("Lobby warehouse store replays idempotently"), StoreReplay.bWasReplay);
	TestEqual(TEXT("Replay does not duplicate the warehouse transfer"), LobbyPlayerState->GetWarehouseComponent()->GetItemCount(Transferable.ItemId), 2);

	const FPRItemInstance StoredStack = LobbyPlayerState->GetWarehouseComponent()->GetWarehouseItems()[0];
	FPRItemTransactionRequest RetrieveRequest;
	RetrieveRequest.Header.TransactionId = FGuid::NewGuid();
	RetrieveRequest.Header.ExpectedRevision = LobbyTransactions->GetRevision();
	RetrieveRequest.Intent = EPRItemTransactionIntent::RetrieveFromWarehouse;
	RetrieveRequest.InstanceGuid = StoredStack.InstanceGuid;
	RetrieveRequest.Count = StoredStack.Count;
	const FPRItemTransactionResult RetrievedResult = LobbyTransactions->ExecuteAuthoritativeTransaction(RetrieveRequest);
	TestEqual(TEXT("Lobby retrieve transfer succeeds authoritatively"), RetrievedResult.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Retrieve returns the total item quantity to the backpack"), LobbyPlayerState->GetInventoryComponent()->GetItemCount(Transferable.ItemId), 3);
	TestEqual(TEXT("Retrieve clears the warehouse stack"), LobbyPlayerState->GetWarehouseComponent()->GetItemCount(Transferable.ItemId), 0);

	return true;
}

#endif
