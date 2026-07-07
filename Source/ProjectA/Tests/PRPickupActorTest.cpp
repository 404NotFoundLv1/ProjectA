#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DefaultPawn.h"
#include "Items/PRPickupActor.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRPickupActorTest, "ProjectRift.Items.PickupActor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UScriptStruct* GetPickupActorItemInstanceStruct()
{
	return FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemInstance"));
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
		TEXT("Pickup prompt is lifted clear of the pickup mesh"),
		InteractionPromptWidget && InteractionPromptWidget->GetRelativeLocation().Z >= 220.0f);

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

	FPRItemInstance TestItem;
	TestItem.ItemId = TEXT("EnergyCrystal");
	TestItem.Count = 3;
	Pickup->SetItemInstance(TestItem);
	TestEqual(TEXT("Pickup stores assigned item id"), Pickup->GetItemInstance().ItemId, FName(TEXT("EnergyCrystal")));
	TestEqual(TEXT("Pickup stores assigned item count"), Pickup->GetItemInstance().Count, 3);
	TestTrue(TEXT("Pickup with valid item is pickable"), Pickup->CanBePickedUp());

	UWidgetComponent* RuntimeInteractionPromptWidget = FindObject<UWidgetComponent>(Pickup, TEXT("InteractionPromptWidget"));
	ADefaultPawn* NearbyPawn = World->SpawnActor<ADefaultPawn>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn nearby pawn for pickup prompt"), NearbyPawn);
	if (RuntimeInteractionPromptWidget)
	{
		RuntimeInteractionPromptWidget->SetHiddenInGame(true);
		RuntimeInteractionPromptWidget->SetVisibility(false, true);
	}
	Pickup->Tick(0.0f);
	TestFalse(
		TEXT("Pickup prompt widget becomes visible when a pawn is already nearby"),
		RuntimeInteractionPromptWidget ? RuntimeInteractionPromptWidget->bHiddenInGame : true);
	TestTrue(
		TEXT("Pickup prompt widget component becomes visible when a pawn is already nearby"),
		RuntimeInteractionPromptWidget && RuntimeInteractionPromptWidget->IsVisible());

	Pickup->SetPickedUp(true);
	TestTrue(TEXT("Pickup stores picked-up state"), Pickup->IsPickedUp());
	TestFalse(TEXT("Already picked-up actor is not pickable"), Pickup->CanBePickedUp());

	return true;
}

#endif
