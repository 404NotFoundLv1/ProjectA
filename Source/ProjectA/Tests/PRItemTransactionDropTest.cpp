#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTransactionComponent.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemTransactionDropTest,
	"ProjectRift.Items.AuthoritativeTransactions.Drop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemTransactionDropTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Transaction drop world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APRPickupActor* Destination = World->SpawnActor<APRPickupActor>();
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UPRItemTransactionComponent* Transactions = PlayerState ? PlayerState->GetItemTransactionComponent() : nullptr;
	TestNotNull(TEXT("Transaction drop PlayerState exists"), PlayerState);
	TestNotNull(TEXT("Transaction drop destination exists"), Destination);
	TestNotNull(TEXT("Transaction drop inventory exists"), Inventory);
	TestNotNull(TEXT("Transaction drop component exists"), Transactions);
	if (!Inventory || !Transactions || !Destination)
	{
		return false;
	}

	FPRItemInstance Stack;
	Stack.ItemId = TEXT("TransactionDropChip");
	Stack.Count = 2;
	TestTrue(TEXT("Transaction drop seeds a stack"), Inventory->AddItem(Stack));
	const TArray<FPRItemInstance> SeededItems = Inventory->GetInventoryItems();
	if (SeededItems.Num() != 1)
	{
		return false;
	}

	FPRItemTransactionRequest MissingDestination;
	MissingDestination.Header.TransactionId = FGuid::NewGuid();
	MissingDestination.Header.ExpectedRevision = 0;
	MissingDestination.Intent = EPRItemTransactionIntent::Drop;
	MissingDestination.InstanceGuid = SeededItems[0].InstanceGuid;
	MissingDestination.Count = 1;
	const FPRItemTransactionResult MissingDestinationResult = Transactions->ExecuteAuthoritativeTransaction(MissingDestination);
	TestEqual(TEXT("Drop without a prepared world destination is rejected"), MissingDestinationResult.Status, EPRItemTransactionStatus::InvalidRequest);
	TestEqual(TEXT("Rejected drop leaves stack untouched"), Inventory->GetInventoryItems()[0].Count, 2);

	FPRItemInstance SplitItem = SeededItems[0];
	SplitItem.Count = 1;
	SplitItem.InstanceGuid = FGuid::NewGuid();
	Destination->SetItemInstance(SplitItem);
	FPRItemTransactionRequest DropRequest;
	DropRequest.Header.TransactionId = FGuid::NewGuid();
	DropRequest.Header.ExpectedRevision = 0;
	DropRequest.Intent = EPRItemTransactionIntent::Drop;
	DropRequest.InstanceGuid = SeededItems[0].InstanceGuid;
	DropRequest.Count = 1;
	DropRequest.TargetActor = Destination;
	const FPRItemTransactionResult DropResult = Transactions->ExecuteAuthoritativeTransaction(DropRequest);
	TestEqual(TEXT("Prepared GUID split drop commits"), DropResult.Status, EPRItemTransactionStatus::Success);
	TestTrue(TEXT("Split drop reports its newly created world identity"), DropResult.CreatedInstanceGuids.Contains(SplitItem.InstanceGuid));
	TestEqual(TEXT("Split drop increments revision once"), Transactions->GetRevision(), 1);
	const TArray<FPRItemInstance> RemainingItems = Inventory->GetInventoryItems();
	TestEqual(TEXT("Split drop preserves the original stack instance"), RemainingItems[0].InstanceGuid, SeededItems[0].InstanceGuid);
	TestEqual(TEXT("Split drop removes only the requested count"), RemainingItems[0].Count, 1);

	return true;
}

#endif
