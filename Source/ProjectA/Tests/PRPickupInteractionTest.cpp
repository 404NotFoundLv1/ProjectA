#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRPickupInteractionTest, "ProjectRift.Items.PickupInteraction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRItemInstance MakePickupInteractionTestItem(const FName ItemId, const int32 Count = 1)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}

void SetPickupInteractionIntProperty(UObject* Object, const TCHAR* PropertyName, const int32 Value)
{
	if (FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), PropertyName) : nullptr)
	{
		Property->SetPropertyValue_InContainer(Object, Value);
	}
}

APRPlayerController* SpawnPickupTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APawn* Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Can spawn ProjectRift player controller"), PlayerController);
	Test.TestNotNull(TEXT("Can spawn ProjectRift player state"), PlayerState);
	Test.TestNotNull(TEXT("Can spawn test pawn"), Pawn);

	if (!PlayerController || !PlayerState || !Pawn)
	{
		return nullptr;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(Pawn);

	Test.TestNotNull(TEXT("Test player owns an inventory component"), PlayerState->GetInventoryComponent());
	return PlayerController;
}

APRPickupActor* SpawnPickup(FAutomationTestBase& Test, UWorld* World, const FVector& Location, const FPRItemInstance& Item)
{
	if (!World)
	{
		return nullptr;
	}

	APRPickupActor* Pickup = World->SpawnActor<APRPickupActor>(Location, FRotator::ZeroRotator);
	Test.TestNotNull(TEXT("Can spawn pickup actor"), Pickup);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->SetItemInstance(Item);
	return Pickup;
}

UPRInventoryComponent* GetInventory(const APRPlayerController* PlayerController)
{
	const APRPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APRPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}
}

bool FPRPickupInteractionTest::RunTest(const FString& Parameters)
{
	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("APRPlayerController class exists"), PlayerControllerClass);
	if (!PlayerControllerClass)
	{
		return false;
	}

	TestNotNull(TEXT("TryPickup input entry point exists"), PlayerControllerClass->FindFunctionByName(TEXT("TryPickup")));
	TestNotNull(TEXT("FindBestPickupCandidate helper exists"), PlayerControllerClass->FindFunctionByName(TEXT("FindBestPickupCandidate")));

	UFunction* ServerTryPickupFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerTryPickup"));
	TestNotNull(TEXT("ServerTryPickup RPC exists"), ServerTryPickupFunction);
	TestTrue(
		TEXT("ServerTryPickup is server-authoritative"),
		ServerTryPickupFunction && ServerTryPickupFunction->HasAnyFunctionFlags(FUNC_NetServer));

	FObjectProperty* PickupParameter = ServerTryPickupFunction ? FindFProperty<FObjectProperty>(ServerTryPickupFunction, TEXT("PickupActor")) : nullptr;
	TestTrue(
		TEXT("ServerTryPickup accepts an APRPickupActor parameter"),
		PickupParameter && PickupParameter->PropertyClass && PickupParameter->PropertyClass->IsChildOf(APRPickupActor::StaticClass()));

	FFloatProperty* RadiusProperty = FindFProperty<FFloatProperty>(PlayerControllerClass, TEXT("PickupInteractionRadius"));
	TestNotNull(TEXT("Player controller exposes pickup interaction radius"), RadiusProperty);
	const APRPlayerController* PlayerControllerDefaults = Cast<APRPlayerController>(PlayerControllerClass->GetDefaultObject());
	TestTrue(
		TEXT("Pickup interaction radius is usable"),
		RadiusProperty && PlayerControllerDefaults && RadiusProperty->GetPropertyValue_InContainer(PlayerControllerDefaults) >= 100.0f);

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	APRPlayerController* FirstPlayer = SpawnPickupTestPlayer(*this, World, FVector::ZeroVector);
	UPRInventoryComponent* FirstInventory = GetInventory(FirstPlayer);
	TestNotNull(TEXT("First player inventory is available"), FirstInventory);
	if (!FirstPlayer || !FirstInventory)
	{
		return false;
	}

	APRPickupActor* NearbyPickup = SpawnPickup(*this, World, FVector(80.0, 0.0, 0.0), MakePickupInteractionTestItem(TEXT("EnergyCrystal"), 2));
	TestTrue(TEXT("Server accepts a valid nearby pickup"), FirstPlayer->TryPickupOnServer(NearbyPickup));
	TestEqual(TEXT("Successful pickup adds item to inventory"), FirstInventory->GetItemCount(TEXT("EnergyCrystal")), 2);
	TestTrue(TEXT("Successful pickup marks pickup as consumed"), NearbyPickup && NearbyPickup->IsPickedUp());
	TestTrue(TEXT("Successful pickup destroys the pickup actor"), NearbyPickup && NearbyPickup->IsActorBeingDestroyed());

	APRPlayerController* RaceWinner = SpawnPickupTestPlayer(*this, World, FVector(300.0, 0.0, 0.0));
	APRPlayerController* RaceLoser = SpawnPickupTestPlayer(*this, World, FVector(300.0, 80.0, 0.0));
	UPRInventoryComponent* WinnerInventory = GetInventory(RaceWinner);
	UPRInventoryComponent* LoserInventory = GetInventory(RaceLoser);
	APRPickupActor* SharedPickup = SpawnPickup(*this, World, FVector(340.0, 0.0, 0.0), MakePickupInteractionTestItem(TEXT("RiftShard"), 1));

	TestTrue(TEXT("First player can claim shared pickup"), RaceWinner && RaceWinner->TryPickupOnServer(SharedPickup));
	TestFalse(TEXT("Second player cannot claim an already consumed pickup"), RaceLoser && RaceLoser->TryPickupOnServer(SharedPickup));
	TestEqual(TEXT("Race winner receives shared pickup"), WinnerInventory ? WinnerInventory->GetItemCount(TEXT("RiftShard")) : INDEX_NONE, 1);
	TestEqual(TEXT("Race loser receives nothing"), LoserInventory ? LoserInventory->GetItemCount(TEXT("RiftShard")) : INDEX_NONE, 0);

	APRPlayerController* FullInventoryPlayer = SpawnPickupTestPlayer(*this, World, FVector(700.0, 0.0, 0.0));
	UPRInventoryComponent* FullInventory = GetInventory(FullInventoryPlayer);
	TestNotNull(TEXT("Full inventory player inventory is available"), FullInventory);
	if (!FullInventoryPlayer || !FullInventory)
	{
		return false;
	}

	SetPickupInteractionIntProperty(FullInventory, TEXT("MaxSlots"), 1);
	TestTrue(TEXT("Can seed full inventory"), FullInventory->AddItem(MakePickupInteractionTestItem(TEXT("HealthInjector"), 1)));
	APRPickupActor* FullInventoryPickup = SpawnPickup(*this, World, FVector(760.0, 0.0, 0.0), MakePickupInteractionTestItem(TEXT("ShieldPack"), 1));

	TestFalse(TEXT("Server rejects pickup when inventory is full"), FullInventoryPlayer->TryPickupOnServer(FullInventoryPickup));
	TestEqual(TEXT("Full inventory did not receive rejected item"), FullInventory->GetItemCount(TEXT("ShieldPack")), 0);
	TestTrue(TEXT("Rejected full-inventory pickup remains available"), FullInventoryPickup && FullInventoryPickup->CanBePickedUp());
	TestFalse(TEXT("Rejected full-inventory pickup is not destroyed"), FullInventoryPickup && FullInventoryPickup->IsActorBeingDestroyed());

	APRPickupActor* FarPickup = SpawnPickup(*this, World, FVector(1400.0, 0.0, 0.0), MakePickupInteractionTestItem(TEXT("DistantCache"), 1));
	TestFalse(TEXT("Server rejects pickup outside interaction range"), FirstPlayer->TryPickupOnServer(FarPickup));
	TestEqual(TEXT("Out-of-range item is not added"), FirstInventory->GetItemCount(TEXT("DistantCache")), 0);
	TestTrue(TEXT("Out-of-range pickup remains available"), FarPickup && FarPickup->CanBePickedUp());

	return true;
}

#endif
