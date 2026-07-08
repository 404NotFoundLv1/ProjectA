#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DefaultPawn.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRPickupActorTest, "ProjectRift.Items.PickupActor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UScriptStruct* GetPickupActorItemInstanceStruct()
{
	return FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemInstance"));
}

FPRItemInstance MakePickupActorTestItem(const FName ItemId, const int32 Count = 1)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}

void SetPickupActorTestIntProperty(UObject* Object, const TCHAR* PropertyName, const int32 Value)
{
	if (FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), PropertyName) : nullptr)
	{
		Property->SetPropertyValue_InContainer(Object, Value);
	}
}

APRPlayerController* SpawnPickupPromptTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Can spawn pickup prompt player controller"), PlayerController);
	Test.TestNotNull(TEXT("Can spawn pickup prompt player state"), PlayerState);
	Test.TestNotNull(TEXT("Can spawn pickup prompt pawn"), Pawn);
	if (!PlayerController || !PlayerState || !Pawn)
	{
		return nullptr;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(Pawn);
	return PlayerController;
}
}

bool FPRPickupActorTest::RunTest(const FString& Parameters)
{
	UClass* PickupActorClass = APRPickupActor::StaticClass();
	TestNotNull(TEXT("APRPickupActor class exists"), PickupActorClass);
	TestTrue(
		TEXT("APRPickupActor derives from AActor"),
		PickupActorClass && PickupActorClass->IsChildOf(AActor::StaticClass()));

	UScriptStruct* ItemInstanceStruct = GetPickupActorItemInstanceStruct();
	TestNotNull(TEXT("FPRItemInstance struct is available to pickup actor"), ItemInstanceStruct);

	if (!PickupActorClass || !ItemInstanceStruct)
	{
		return false;
	}

	TestNotNull(TEXT("GetItemInstance function exists"), PickupActorClass->FindFunctionByName(TEXT("GetItemInstance")));
	TestNotNull(TEXT("SetItemInstance function exists"), PickupActorClass->FindFunctionByName(TEXT("SetItemInstance")));
	TestNotNull(TEXT("CanBePickedUp function exists"), PickupActorClass->FindFunctionByName(TEXT("CanBePickedUp")));
	TestNotNull(TEXT("SetPickedUp function exists"), PickupActorClass->FindFunctionByName(TEXT("SetPickedUp")));
	TestNotNull(TEXT("GetInteractionPromptText function exists"), PickupActorClass->FindFunctionByName(TEXT("GetInteractionPromptText")));

	FStructProperty* ItemInstanceProperty = FindFProperty<FStructProperty>(PickupActorClass, TEXT("ItemInstance"));
	TestNotNull(TEXT("Pickup actor owns ItemInstance"), ItemInstanceProperty);
	TestTrue(
		TEXT("Pickup ItemInstance uses FPRItemInstance"),
		ItemInstanceProperty && ItemInstanceProperty->Struct == ItemInstanceStruct);
	TestTrue(
		TEXT("Pickup ItemInstance is replicated for client display"),
		ItemInstanceProperty && ItemInstanceProperty->HasAnyPropertyFlags(CPF_Net));

	FBoolProperty* PickedUpProperty = FindFProperty<FBoolProperty>(PickupActorClass, TEXT("bIsPickedUp"));
	TestNotNull(TEXT("Pickup actor owns bIsPickedUp state"), PickedUpProperty);
	TestTrue(
		TEXT("Pickup picked-up state is replicated"),
		PickedUpProperty && PickedUpProperty->HasAnyPropertyFlags(CPF_Net));

	AActor* PickupDefaults = Cast<AActor>(PickupActorClass->GetDefaultObject());
	TestNotNull(TEXT("Pickup actor CDO is an actor"), PickupDefaults);
	TestTrue(TEXT("Pickup actor replicates by default"), PickupDefaults && PickupDefaults->GetIsReplicated());

	UStaticMeshComponent* Mesh = PickupDefaults ? FindObject<UStaticMeshComponent>(PickupDefaults, TEXT("Mesh")) : nullptr;
	USphereComponent* InteractionSphere = PickupDefaults ? FindObject<USphereComponent>(PickupDefaults, TEXT("InteractionSphere")) : nullptr;
	UWidgetComponent* InteractionPromptWidget = PickupDefaults ? FindObject<UWidgetComponent>(PickupDefaults, TEXT("InteractionPromptWidget")) : nullptr;
	TestNotNull(TEXT("Pickup actor owns Mesh component"), Mesh);
	TestNotNull(TEXT("Pickup actor owns InteractionSphere component"), InteractionSphere);
	TestNotNull(TEXT("Pickup actor owns a screen-space interaction prompt widget"), InteractionPromptWidget);
	TestTrue(TEXT("Pickup mesh has a visible default mesh"), Mesh && Mesh->GetStaticMesh() != nullptr);
	TestTrue(TEXT("Interaction sphere has usable radius"), InteractionSphere && InteractionSphere->GetUnscaledSphereRadius() >= 100.0f);
	TestTrue(TEXT("Interaction sphere uses a readable pickup prompt radius"), InteractionSphere && InteractionSphere->GetUnscaledSphereRadius() >= 300.0f);
	TestTrue(TEXT("Interaction sphere is query only"), InteractionSphere && InteractionSphere->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestEqual(
		TEXT("Interaction sphere overlaps pawns"),
		InteractionSphere ? InteractionSphere->GetCollisionResponseToChannel(ECC_Pawn) : ECR_Ignore,
		ECR_Overlap);
	TestTrue(TEXT("Pickup prompt widget is hidden until a pawn is nearby"), InteractionPromptWidget && InteractionPromptWidget->bHiddenInGame);
	TestEqual(
		TEXT("Pickup prompt uses screen-space widget rendering for localized text"),
		InteractionPromptWidget ? InteractionPromptWidget->GetWidgetSpace() : EWidgetSpace::World,
		EWidgetSpace::Screen);
	TestTrue(
		TEXT("Pickup prompt widget has a native widget class"),
		InteractionPromptWidget && InteractionPromptWidget->GetWidgetClass() != nullptr);
	TestTrue(
		TEXT("Pickup prompt defaults close to the pickup instead of near the top of the screen"),
		InteractionPromptWidget
		&& InteractionPromptWidget->GetRelativeLocation().Z >= 80.0f
		&& InteractionPromptWidget->GetRelativeLocation().Z <= 160.0f);

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	APRPickupActor* Pickup = World->SpawnActor<APRPickupActor>();
	TestNotNull(TEXT("Can spawn pickup actor in a map"), Pickup);
	if (!Pickup)
	{
		return false;
	}

	TestFalse(TEXT("Pickup with invalid item is not pickable"), Pickup->CanBePickedUp());

	FPRItemInstance TestItem = MakePickupActorTestItem(TEXT("EnergyCrystal"), 3);
	Pickup->SetItemInstance(TestItem);
	TestEqual(TEXT("Pickup stores assigned item id"), Pickup->GetItemInstance().ItemId, FName(TEXT("EnergyCrystal")));
	TestEqual(TEXT("Pickup stores assigned item count"), Pickup->GetItemInstance().Count, 3);
	TestTrue(TEXT("Pickup with valid item is pickable"), Pickup->CanBePickedUp());
	TestTrue(
		TEXT("Pickup prompt uses the F interact key and readable pickup text"),
		Pickup->GetInteractionPromptText().ToString().Contains(TEXT("F"))
		&& Pickup->GetInteractionPromptText().ToString().Contains(TEXT("\u62FE\u53D6")));

	UWidgetComponent* RuntimeInteractionPromptWidget = FindObject<UWidgetComponent>(Pickup, TEXT("InteractionPromptWidget"));
	APRPlayerController* LocalPromptPlayer = SpawnPickupPromptTestPlayer(*this, World, FVector(1000.0f, 0.0f, 0.0f));
	APRPlayerController* OtherNearbyPlayer = SpawnPickupPromptTestPlayer(*this, World, FVector(80.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("Can spawn local prompt player for pickup prompt"), LocalPromptPlayer);
	TestNotNull(TEXT("Can spawn another nearby player for pickup prompt isolation"), OtherNearbyPlayer);
	if (RuntimeInteractionPromptWidget)
	{
		RuntimeInteractionPromptWidget->SetHiddenInGame(true);
		RuntimeInteractionPromptWidget->SetVisibility(false, true);
	}
	Pickup->Tick(0.0f);
	TestTrue(
		TEXT("Pickup prompt stays hidden when only another player is nearby"),
		RuntimeInteractionPromptWidget ? RuntimeInteractionPromptWidget->bHiddenInGame : false);

	if (APawn* LocalPromptPawn = LocalPromptPlayer ? LocalPromptPlayer->GetPawn() : nullptr)
	{
		LocalPromptPawn->SetActorLocation(FVector(80.0f, 0.0f, 0.0f));
	}
	if (RuntimeInteractionPromptWidget)
	{
		RuntimeInteractionPromptWidget->SetHiddenInGame(true);
		RuntimeInteractionPromptWidget->SetVisibility(false, true);
	}
	Pickup->Tick(0.0f);
	TestFalse(
		TEXT("Pickup prompt widget becomes visible when the local player is nearby"),
		RuntimeInteractionPromptWidget ? RuntimeInteractionPromptWidget->bHiddenInGame : true);
	TestTrue(
		TEXT("Pickup prompt widget component becomes visible when the local player is nearby"),
		RuntimeInteractionPromptWidget && RuntimeInteractionPromptWidget->IsVisible());

	APRPickupActor* ReadableRangePickup = World->SpawnActor<APRPickupActor>(FVector(1800.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn pickup for readable-range prompt test"), ReadableRangePickup);
	if (APawn* LocalPromptPawn = LocalPromptPlayer ? LocalPromptPlayer->GetPawn() : nullptr)
	{
		LocalPromptPawn->SetActorLocation(FVector(1500.0f, 0.0f, 0.0f));
	}
	if (ReadableRangePickup && LocalPromptPlayer)
	{
		ReadableRangePickup->SetItemInstance(MakePickupActorTestItem(TEXT("EnergyCrystal"), 1));
		UWidgetComponent* ReadableRangePromptWidget = FindObject<UWidgetComponent>(ReadableRangePickup, TEXT("InteractionPromptWidget"));
		if (ReadableRangePromptWidget)
		{
			ReadableRangePromptWidget->SetHiddenInGame(true);
			ReadableRangePromptWidget->SetVisibility(false, true);
		}

		ReadableRangePickup->Tick(0.0f);
		TestFalse(
			TEXT("Pickup prompt appears at readable interaction range when item can be picked up"),
			ReadableRangePromptWidget ? ReadableRangePromptWidget->bHiddenInGame : true);
		TestTrue(
			TEXT("Pickup prompt widget is visible at readable interaction range when item can be picked up"),
			ReadableRangePromptWidget && ReadableRangePromptWidget->IsVisible());
	}

	if (APawn* LocalPromptPawn = LocalPromptPlayer ? LocalPromptPlayer->GetPawn() : nullptr)
	{
		LocalPromptPawn->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
	}
	APRPlayerState* FullInventoryPlayerState = LocalPromptPlayer ? LocalPromptPlayer->GetPlayerState<APRPlayerState>() : nullptr;
	UPRInventoryComponent* FullInventory = FullInventoryPlayerState ? FullInventoryPlayerState->GetInventoryComponent() : nullptr;
	APRPickupActor* BlockedPickup = World->SpawnActor<APRPickupActor>(FVector(1080.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn pickup for full-inventory prompt test"), BlockedPickup);
	TestNotNull(TEXT("Full-inventory player owns inventory"), FullInventory);
	if (BlockedPickup && FullInventory)
	{
		SetPickupActorTestIntProperty(FullInventory, TEXT("MaxSlots"), 1);
		TestTrue(TEXT("Can fill prompt test inventory"), FullInventory->AddItem(MakePickupActorTestItem(TEXT("HealthInjector"), 1)));
		BlockedPickup->SetItemInstance(MakePickupActorTestItem(TEXT("ShieldPack"), 1));

		UWidgetComponent* BlockedPromptWidget = FindObject<UWidgetComponent>(BlockedPickup, TEXT("InteractionPromptWidget"));
		if (BlockedPromptWidget)
		{
			BlockedPromptWidget->SetHiddenInGame(true);
			BlockedPromptWidget->SetVisibility(false, true);
		}

		BlockedPickup->Tick(0.0f);
		TestTrue(
			TEXT("Pickup prompt stays hidden when nearby player cannot add the item"),
			BlockedPromptWidget ? BlockedPromptWidget->bHiddenInGame : false);
		TestFalse(
			TEXT("Pickup prompt widget component stays invisible when nearby player cannot add the item"),
			BlockedPromptWidget && BlockedPromptWidget->IsVisible());
	}

	Pickup->SetPickedUp(true);
	TestTrue(TEXT("Pickup stores picked-up state"), Pickup->IsPickedUp());
	TestFalse(TEXT("Already picked-up actor is not pickable"), Pickup->CanBePickedUp());

	return true;
}

#endif
