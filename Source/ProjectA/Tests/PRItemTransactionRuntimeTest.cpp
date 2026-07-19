#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Items/PRItemTransactionComponent.h"
#include "Items/PRInventoryComponent.h"
#include "Player/PRPlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemTransactionRuntimeTest,
	"ProjectRift.Items.AuthoritativeTransactions.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemTransactionRuntimeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<APRPlayerState> PlayerState(NewObject<APRPlayerState>(GetTransientPackage()));
	UPRItemTransactionComponent* Transactions = PlayerState->GetItemTransactionComponent();
	TestNotNull(TEXT("PlayerState owns a transaction component"), Transactions);
	if (!Transactions)
	{
		return false;
	}

	FPRItemTransactionRequest ReloadRequest;
	ReloadRequest.Header.TransactionId = FGuid::NewGuid();
	ReloadRequest.Header.ExpectedRevision = 0;
	ReloadRequest.Intent = EPRItemTransactionIntent::BeginReload;

	const FPRItemTransactionResult PendingResult = Transactions->ExecuteAuthoritativeTransaction(ReloadRequest);
	TestEqual(TEXT("Reload request enters pending state"), PendingResult.Status, EPRItemTransactionStatus::Pending);
	TestEqual(TEXT("Pending reload does not increment revision"), PendingResult.CurrentRevision, 0);
	TestEqual(TEXT("Component revision remains unchanged while reload is pending"), Transactions->GetRevision(), 0);

	const FPRItemTransactionResult ReplayResult = Transactions->ExecuteAuthoritativeTransaction(ReloadRequest);
	TestEqual(TEXT("Duplicate pending reload remains pending"), ReplayResult.Status, EPRItemTransactionStatus::Pending);
	TestTrue(TEXT("Duplicate pending reload is marked replayed"), ReplayResult.bWasReplay);
	TestEqual(TEXT("Duplicate pending reload does not increment revision"), Transactions->GetRevision(), 0);

	FPRItemTransactionRequest StaleRequest;
	StaleRequest.Header.TransactionId = FGuid::NewGuid();
	StaleRequest.Header.ExpectedRevision = 1;
	StaleRequest.Intent = EPRItemTransactionIntent::BeginReload;
	const FPRItemTransactionResult StaleResult = Transactions->ExecuteAuthoritativeTransaction(StaleRequest);
	TestEqual(TEXT("Mismatched revision is rejected"), StaleResult.Status, EPRItemTransactionStatus::RevisionConflict);
	TestEqual(TEXT("Rejected request returns authoritative revision"), StaleResult.CurrentRevision, 0);

	UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	FPRItemInstance Consumable;
	Consumable.ItemId = TEXT("TransactionConsumable");
	Consumable.Count = 2;
	TestTrue(TEXT("Server inventory accepts a transaction test stack"), Inventory && Inventory->AddItem(Consumable));
	const TArray<FPRItemInstance> BeforeUse = Inventory ? Inventory->GetInventoryItems() : TArray<FPRItemInstance>();
	TestEqual(TEXT("Test stack exists before transaction use"), BeforeUse.Num(), 1);
	if (BeforeUse.Num() == 1)
	{
		FPRItemTransactionRequest UseRequest;
		UseRequest.Header.TransactionId = FGuid::NewGuid();
		UseRequest.Header.ExpectedRevision = 0;
		UseRequest.Intent = EPRItemTransactionIntent::Use;
		UseRequest.InstanceGuid = BeforeUse[0].InstanceGuid;
		UseRequest.Count = 1;
		const FPRItemTransactionResult UseResult = Transactions->ExecuteAuthoritativeTransaction(UseRequest);
		TestEqual(TEXT("GUID-targeted use commits successfully"), UseResult.Status, EPRItemTransactionStatus::Success);
		TestEqual(TEXT("Successful use increments revision once"), Transactions->GetRevision(), 1);
		const TArray<FPRItemInstance> AfterUse = Inventory->GetInventoryItems();
		TestEqual(TEXT("Use keeps the stack when quantity remains"), AfterUse.Num(), 1);
		if (AfterUse.Num() == 1)
		{
			TestEqual(TEXT("Use decrements only the requested stack"), AfterUse[0].Count, 1);
			TestEqual(TEXT("Partial use preserves instance identity"), AfterUse[0].InstanceGuid, UseRequest.InstanceGuid);
		}
	}

	FPRItemInstance Ammo;
	Ammo.ItemId = TEXT("TransactionAmmo");
	Ammo.Count = 5;
	TestTrue(TEXT("Server inventory accepts reload ammo"), Inventory && Inventory->AddItem(Ammo));
	FPRItemTransactionRequest ReloadCompletionRequest;
	ReloadCompletionRequest.Header.TransactionId = FGuid::NewGuid();
	ReloadCompletionRequest.Header.ExpectedRevision = Transactions->GetRevision();
	ReloadCompletionRequest.Intent = EPRItemTransactionIntent::BeginReload;
	const FPRItemTransactionResult ReloadPending = Transactions->ExecuteAuthoritativeTransaction(ReloadCompletionRequest);
	TestEqual(TEXT("Reload completion transaction begins pending"), ReloadPending.Status, EPRItemTransactionStatus::Pending);
	TestEqual(TEXT("Reload start does not remove reserve ammo"), Inventory->GetItemCount(TEXT("TransactionAmmo")), 5);
	const FPRItemTransactionResult ReloadComplete = Transactions->CompletePendingReload(
		ReloadCompletionRequest.Header.TransactionId,
		TEXT("TransactionAmmo"),
		3);
	TestEqual(TEXT("Reload completion commits through the transaction authority"), ReloadComplete.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Reload completion reports the actual committed ammo"), ReloadComplete.AppliedCount, 3);
	TestEqual(TEXT("Reload completion increments revision once"), Transactions->GetRevision(), 2);
	TestEqual(TEXT("Reload completion removes only actual committed ammo"), Inventory->GetItemCount(TEXT("TransactionAmmo")), 2);

	return true;
}

#endif
