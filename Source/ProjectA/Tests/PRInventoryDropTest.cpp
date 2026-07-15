#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UI/PRInventoryWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRInventoryDropTest, "ProjectRift.Items.DropFromInventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRItemInstance MakeInventoryDropTestItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Level = 2;
	Item.Rarity = EPRItemRarity::Rare;
	return Item;
}

APRPlayerController* SpawnInventoryDropTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location, const FRotator& Rotation)
{
	if (!World)
	{
		return nullptr;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APawn* Pawn = World->SpawnActor<ADefaultPawn>(Location, Rotation);

	Test.TestNotNull(TEXT("Drop test controller spawned"), PlayerController);
	Test.TestNotNull(TEXT("Drop test player state spawned"), PlayerState);
	Test.TestNotNull(TEXT("Drop test pawn spawned"), Pawn);
	if (!PlayerController || !PlayerState || !Pawn)
	{
		return nullptr;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(Pawn);

	Test.TestNotNull(TEXT("Drop test player owns inventory"), PlayerState->GetInventoryComponent());
	return PlayerController;
}

UPRInventoryComponent* GetInventoryForDropTest(const APRPlayerController* PlayerController)
{
	const APRPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APRPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}

TArray<APRPickupActor*> GetAvailablePickups(UWorld* World)
{
	TArray<APRPickupActor*> Pickups;
	if (!World)
	{
		return Pickups;
	}

	for (TActorIterator<APRPickupActor> It(World); It; ++It)
	{
		APRPickupActor* Pickup = *It;
		if (Pickup && Pickup->CanBePickedUp() && !Pickup->IsActorBeingDestroyed())
		{
			Pickups.Add(Pickup);
		}
	}

	return Pickups;
}
}

bool FPRInventoryDropTest::RunTest(const FString& Parameters)
{
	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Player controller exposes DropInventoryItem"), PlayerControllerClass->FindFunctionByName(TEXT("DropInventoryItem")));
	TestNotNull(TEXT("Player controller exposes ServerDropInventoryItem"), PlayerControllerClass->FindFunctionByName(TEXT("ServerDropInventoryItem")));
	UFunction* ServerDropFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerDropInventoryItem"));
	TestTrue(TEXT("ServerDropInventoryItem is a server RPC"), ServerDropFunction && ServerDropFunction->HasAnyFunctionFlags(FUNC_NetServer));

	UClass* InventoryWidgetClass = UPRInventoryWidget::StaticClass();
	TestNotNull(TEXT("Inventory widget exposes RequestDropSelectedItem"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestDropSelectedItem")));
	TestNotNull(TEXT("Inventory widget exposes RequestDropDisplayedItem"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestDropDisplayedItem")));
	TestNotNull(TEXT("Inventory widget exposes OnDropItemRequested delegate"), FindFProperty<FMulticastDelegateProperty>(InventoryWidgetClass, TEXT("OnDropItemRequested")));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	APRPlayerController* Dropper = SpawnInventoryDropTestPlayer(*this, World, FVector::ZeroVector, FRotator(0.0f, 90.0f, 0.0f));
	UPRInventoryComponent* DropperInventory = GetInventoryForDropTest(Dropper);
	TestNotNull(TEXT("Dropper inventory exists"), DropperInventory);
	if (!Dropper || !DropperInventory)
	{
		return false;
	}

	TestTrue(TEXT("Can seed droppable stack"), DropperInventory->AddItem(MakeInventoryDropTestItem(TEXT("EnergyCrystal"), 5)));
	TestEqual(TEXT("No pickup exists before dropping"), GetAvailablePickups(World).Num(), 0);
	TestTrue(TEXT("Server accepts valid inventory drop"), Dropper->TryDropInventoryItemOnServer(TEXT("EnergyCrystal"), 2));
	TestEqual(TEXT("Dropping removes requested item count"), DropperInventory->GetItemCount(TEXT("EnergyCrystal")), 3);

	TArray<APRPickupActor*> Pickups = GetAvailablePickups(World);
	TestEqual(TEXT("Dropping creates one available pickup"), Pickups.Num(), 1);
	APRPickupActor* DroppedPickup = Pickups.Num() == 1 ? Pickups[0] : nullptr;
	TestNotNull(TEXT("Dropped pickup exists"), DroppedPickup);
	if (!DroppedPickup)
	{
		return false;
	}

	const FPRItemInstance DroppedItem = DroppedPickup->GetItemInstance();
	TestEqual(TEXT("Dropped pickup keeps ItemId"), DroppedItem.ItemId, FName(TEXT("EnergyCrystal")));
	TestEqual(TEXT("Dropped pickup uses requested count"), DroppedItem.Count, 2);
	TestEqual(TEXT("Dropped pickup preserves item level"), DroppedItem.Level, 2);
	TestEqual(TEXT("Dropped pickup preserves item rarity"), DroppedItem.Rarity, EPRItemRarity::Rare);
	TestTrue(TEXT("Dropped pickup replicates"), DroppedPickup->GetIsReplicated());

	const APawn* DropperPawn = Dropper->GetPawn();
	const FVector DropOffset = DroppedPickup->GetActorLocation() - (DropperPawn ? DropperPawn->GetActorLocation() : FVector::ZeroVector);
	const FVector ExpectedForward = DropperPawn ? DropperPawn->GetActorForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
	TestTrue(TEXT("Dropped pickup is spawned in front of the player"), FVector::DotProduct(DropOffset.GetSafeNormal2D(), ExpectedForward) > 0.75f);
	TestTrue(TEXT("Dropped pickup is not spawned at the player's feet"), DropOffset.Size2D() >= 80.0f);

	APRPlayerController* Picker = SpawnInventoryDropTestPlayer(*this, World, DroppedPickup->GetActorLocation(), FRotator::ZeroRotator);
	UPRInventoryComponent* PickerInventory = GetInventoryForDropTest(Picker);
	TestNotNull(TEXT("Picker inventory exists"), PickerInventory);
	TestTrue(TEXT("Other player can pick up dropped item"), Picker && Picker->TryPickupOnServer(DroppedPickup));
	TestEqual(TEXT("Other player receives dropped count"), PickerInventory ? PickerInventory->GetItemCount(TEXT("EnergyCrystal")) : INDEX_NONE, 2);
	TestTrue(TEXT("Dropped pickup is consumed after pickup"), DroppedPickup->IsPickedUp());

	const int32 PickupCountAfterClaim = GetAvailablePickups(World).Num();
	TestFalse(TEXT("Cannot drop more than available count"), Dropper->TryDropInventoryItemOnServer(TEXT("EnergyCrystal"), 4));
	TestFalse(TEXT("Cannot drop zero items"), Dropper->TryDropInventoryItemOnServer(TEXT("EnergyCrystal"), 0));
	TestFalse(TEXT("Cannot drop a missing item"), Dropper->TryDropInventoryItemOnServer(TEXT("ShieldPack"), 1));
	TestEqual(TEXT("Rejected drops keep inventory count"), DropperInventory->GetItemCount(TEXT("EnergyCrystal")), 3);
	TestEqual(TEXT("Rejected drops do not create pickups"), GetAvailablePickups(World).Num(), PickupCountAfterClaim);

	UPRItemDataAsset* NonDroppableRifleData = NewObject<UPRItemDataAsset>(GetTransientPackage());
	NonDroppableRifleData->ItemId = TEXT("TestRifle");
	NonDroppableRifleData->ItemType = EPRItemType::Equipment;
	NonDroppableRifleData->MaxStackCount = 1;
	NonDroppableRifleData->bCanDrop = false;
	DropperInventory->SetItemDataAssets({ NonDroppableRifleData });
	TestTrue(TEXT("Can seed non-droppable rifle"), DropperInventory->AddItem(MakeInventoryDropTestItem(TEXT("TestRifle"), 1)));
	TestFalse(TEXT("Non-droppable rifle is rejected"), Dropper->TryDropInventoryItemOnServer(TEXT("TestRifle"), 1));
	TestEqual(TEXT("Rejected non-droppable rifle remains in inventory"), DropperInventory->GetItemCount(TEXT("TestRifle")), 1);
	TestEqual(TEXT("Rejected non-droppable rifle creates no pickup"), GetAvailablePickups(World).Num(), PickupCountAfterClaim);

	return true;
}

#endif
