#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTransactionComponent.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemTransactionPickupTest,
	"ProjectRift.Items.AuthoritativeTransactions.Pickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemTransactionPickupTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Transaction pickup world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APRPickupActor* Pickup = World->SpawnActor<APRPickupActor>();
	TestNotNull(TEXT("Transaction pickup has a PlayerState"), PlayerState);
	TestNotNull(TEXT("Transaction pickup has a world actor"), Pickup);
	if (!PlayerState || !Pickup)
	{
		return false;
	}

	FPRItemInstance Item;
	Item.ItemId = TEXT("TransactionPickupChip");
	Item.Count = 2;
	Pickup->SetItemInstance(Item);
	TestTrue(TEXT("World pickup has a server GUID before transaction"), Pickup->GetItemInstance().HasValidIdentity());

	UPRItemTransactionComponent* Transactions = PlayerState->GetItemTransactionComponent();
	FPRItemTransactionRequest Request;
	Request.Header.TransactionId = FGuid::NewGuid();
	Request.Header.ExpectedRevision = 0;
	Request.Intent = EPRItemTransactionIntent::Pickup;
	Request.TargetActor = Pickup;
	const FPRItemTransactionResult Result = Transactions ? Transactions->ExecuteAuthoritativeTransaction(Request) : FPRItemTransactionResult();
	TestEqual(TEXT("Pickup transaction commits"), Result.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Pickup transaction increments revision once"), Transactions ? Transactions->GetRevision() : INDEX_NONE, 1);
	TestEqual(TEXT("Pickup transaction transfers item count"), PlayerState->GetInventoryComponent()->GetItemCount(TEXT("TransactionPickupChip")), 2);
	TestTrue(TEXT("Pickup transaction marks world actor picked up"), Pickup->IsPickedUp());
	TestTrue(TEXT("Pickup transaction destroys world actor after commit"), Pickup->IsActorBeingDestroyed());

	return true;
}

#endif
