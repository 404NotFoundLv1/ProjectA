#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Items/PREquipmentComponent.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UI/PRInventoryWidget.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRInventoryUITest, "ProjectRift.UI.Inventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRItemInstance MakeInventoryUITestItem(const FName ItemId, const int32 Count, const EPRItemRarity Rarity = EPRItemRarity::Common)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Rarity = Rarity;
	return Item;
}

bool SetInventoryUITestItemDataAssets(FAutomationTestBase& Test, UPRInventoryComponent* Inventory, const TArray<UPRItemDataAsset*>& ItemDataAssets)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("SetItemDataAssets")) : nullptr;
	Test.TestNotNull(TEXT("Inventory exposes SetItemDataAssets for UI display data"), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	FArrayProperty* AssetsProperty = FindFProperty<FArrayProperty>(Function, TEXT("InItemDataAssets"));
	FObjectProperty* InnerObjectProperty = AssetsProperty ? CastField<FObjectProperty>(AssetsProperty->Inner) : nullptr;
	Test.TestNotNull(TEXT("SetItemDataAssets accepts UI item data array"), AssetsProperty);
	Test.TestNotNull(TEXT("SetItemDataAssets UI item data array stores objects"), InnerObjectProperty);
	if (!AssetsProperty || !InnerObjectProperty)
	{
		return false;
	}

	FScriptArrayHelper AssetsHelper(AssetsProperty, AssetsProperty->ContainerPtrToValuePtr<void>(ParamsMemory));
	for (UPRItemDataAsset* ItemDataAsset : ItemDataAssets)
	{
		const int32 NewIndex = AssetsHelper.AddValue();
		InnerObjectProperty->SetObjectPropertyValue(AssetsHelper.GetRawPtr(NewIndex), ItemDataAsset);
	}

	Inventory->ProcessEvent(Function, ParamsMemory);
	return true;
}
}

bool FPRInventoryUITest::RunTest(const FString& Parameters)
{
	UClass* InventoryWidgetClass = UPRInventoryWidget::StaticClass();
	TestNotNull(TEXT("UPRInventoryWidget class exists"), InventoryWidgetClass);
	TestTrue(
		TEXT("UPRInventoryWidget derives from UUserWidget"),
		InventoryWidgetClass && InventoryWidgetClass->IsChildOf(UUserWidget::StaticClass()));

	TestNotNull(TEXT("Inventory widget exposes BindInventory"), InventoryWidgetClass->FindFunctionByName(TEXT("BindInventory")));
	TestNotNull(TEXT("Inventory widget exposes RefreshInventory"), InventoryWidgetClass->FindFunctionByName(TEXT("RefreshInventory")));
	TestNotNull(TEXT("Inventory widget exposes GetDisplayedItems"), InventoryWidgetClass->FindFunctionByName(TEXT("GetDisplayedItems")));
	TestNotNull(TEXT("Inventory widget exposes GetBoundInventory"), InventoryWidgetClass->FindFunctionByName(TEXT("GetBoundInventory")));
	TestNotNull(TEXT("Inventory widget exposes IsInventoryEmpty"), InventoryWidgetClass->FindFunctionByName(TEXT("IsInventoryEmpty")));
	TestNotNull(TEXT("Inventory widget exposes GetItemIcon"), InventoryWidgetClass->FindFunctionByName(TEXT("GetItemIcon")));
	TestNotNull(TEXT("Inventory widget exposes ship resource summary text"), InventoryWidgetClass->FindFunctionByName(TEXT("GetShipResourceSummaryText")));
	TestNotNull(TEXT("Inventory widget exposes primary weapon summary text"), InventoryWidgetClass->FindFunctionByName(TEXT("GetPrimaryWeaponSummaryText")));
	TestNotNull(TEXT("Inventory widget exposes RequestEquipSelectedItem"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestEquipSelectedItem")));
	TestNotNull(TEXT("Inventory widget exposes RequestUnequipPrimaryWeapon"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestUnequipPrimaryWeapon")));
	TestNotNull(TEXT("Inventory widget exposes RequestUnequipEquipmentSlot"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestUnequipEquipmentSlot")));
	TestNotNull(TEXT("Inventory widget exposes OnEquipItemRequested delegate"), FindFProperty<FMulticastDelegateProperty>(InventoryWidgetClass, TEXT("OnEquipItemRequested")));
	TestNotNull(TEXT("Inventory widget exposes OnUnequipPrimaryRequested delegate"), FindFProperty<FMulticastDelegateProperty>(InventoryWidgetClass, TEXT("OnUnequipPrimaryRequested")));
	TestNotNull(TEXT("Inventory widget exposes OnUnequipEquipmentRequested delegate"), FindFProperty<FMulticastDelegateProperty>(InventoryWidgetClass, TEXT("OnUnequipEquipmentRequested")));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("APRPlayerController class exists for inventory UI"), PlayerControllerClass);
	TestNotNull(TEXT("Player controller exposes ToggleInventory"), PlayerControllerClass->FindFunctionByName(TEXT("ToggleInventory")));
	TestNotNull(TEXT("Player controller exposes ShowInventory"), PlayerControllerClass->FindFunctionByName(TEXT("ShowInventory")));
	TestNotNull(TEXT("Player controller exposes HideInventory"), PlayerControllerClass->FindFunctionByName(TEXT("HideInventory")));
	TestNotNull(TEXT("Player controller exposes RefreshInventoryDisplay"), PlayerControllerClass->FindFunctionByName(TEXT("RefreshInventoryDisplay")));
	TestNotNull(TEXT("Player controller exposes IsInventoryVisible"), PlayerControllerClass->FindFunctionByName(TEXT("IsInventoryVisible")));
	TestNotNull(TEXT("Player controller has configurable InventoryWidgetClass"), FindFProperty<FClassProperty>(PlayerControllerClass, TEXT("InventoryWidgetClass")));
	TestNotNull(TEXT("Player controller has configurable WeaponHUDWidgetClass"), FindFProperty<FClassProperty>(PlayerControllerClass, TEXT("WeaponHUDWidgetClass")));
	TestNotNull(TEXT("Player controller exposes GetWeaponHUDWidget"), PlayerControllerClass->FindFunctionByName(TEXT("GetWeaponHUDWidget")));
	TestNotNull(TEXT("Player controller exposes EquipInventoryWeapon"), PlayerControllerClass->FindFunctionByName(TEXT("EquipInventoryWeapon")));
	TestNotNull(TEXT("Player controller exposes UnequipPrimaryWeapon"), PlayerControllerClass->FindFunctionByName(TEXT("UnequipPrimaryWeapon")));
	TestNotNull(TEXT("Player controller exposes UnequipEquipmentSlot"), PlayerControllerClass->FindFunctionByName(TEXT("UnequipEquipmentSlot")));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Inventory UI test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Inventory UI test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	TestNotNull(TEXT("Can spawn inventory UI player controller"), PlayerController);
	if (!PlayerController)
	{
		return false;
	}

	PlayerController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	PlayerController->bShowMouseCursor = false;
	PlayerController->ShowInventory();
	TestTrue(TEXT("Showing inventory makes the inventory visible"), PlayerController->IsInventoryVisible());
	TestTrue(TEXT("Showing inventory displays the mouse cursor for item selection"), PlayerController->bShowMouseCursor);

	UPRInventoryWidget* RuntimeInventoryWidget = PlayerController->GetInventoryWidget();
	TestNotNull(TEXT("Showing inventory creates a runtime inventory widget"), RuntimeInventoryWidget);
	if (RuntimeInventoryWidget)
	{
		const FVector2D Alignment = RuntimeInventoryWidget->GetAlignmentInViewport();
		const FAnchors Anchors = RuntimeInventoryWidget->GetAnchorsInViewport();
		TestTrue(TEXT("Inventory widget is horizontally centered"), FMath::IsNearlyEqual(Alignment.X, 0.5f));
		TestTrue(TEXT("Inventory widget is vertically centered"), FMath::IsNearlyEqual(Alignment.Y, 0.5f));
		TestTrue(TEXT("Inventory widget anchor min X is centered"), FMath::IsNearlyEqual(Anchors.Minimum.X, 0.5f));
		TestTrue(TEXT("Inventory widget anchor min Y is centered"), FMath::IsNearlyEqual(Anchors.Minimum.Y, 0.5f));
		TestTrue(TEXT("Inventory widget anchor max X is centered"), FMath::IsNearlyEqual(Anchors.Maximum.X, 0.5f));
		TestTrue(TEXT("Inventory widget anchor max Y is centered"), FMath::IsNearlyEqual(Anchors.Maximum.Y, 0.5f));
	}

	PlayerController->HideInventory();
	TestFalse(TEXT("Hiding inventory restores the mouse cursor visibility"), PlayerController->bShowMouseCursor);

	UPRInventoryComponent* Inventory = NewObject<UPRInventoryComponent>(GetTransientPackage());
	UPRInventoryWidget* Widget = NewObject<UPRInventoryWidget>(GetTransientPackage(), InventoryWidgetClass);
	TestNotNull(TEXT("Can instantiate inventory component"), Inventory);
	TestNotNull(TEXT("Can instantiate inventory widget"), Widget);
	if (!Inventory || !Widget)
	{
		return false;
	}

	Widget->BindInventory(Inventory);
	TestEqual(TEXT("Widget stores bound inventory"), Widget->GetBoundInventory(), Inventory);
	TestTrue(TEXT("Empty inventory displays no items"), Widget->IsInventoryEmpty());
	TestEqual(TEXT("Displayed items starts empty"), Widget->GetDisplayedItems().Num(), 0);

	TestTrue(TEXT("Inventory accepts first UI test item"), Inventory->AddItem(MakeInventoryUITestItem(TEXT("HealthInjector"), 2, EPRItemRarity::Uncommon)));
	TArray<FPRItemInstance> DisplayedItems = Widget->GetDisplayedItems();
	TestEqual(TEXT("Widget refreshes when inventory changes"), DisplayedItems.Num(), 1);
	if (DisplayedItems.Num() == 1)
	{
		TestEqual(TEXT("Widget displays item id"), DisplayedItems[0].ItemId, FName(TEXT("HealthInjector")));
		TestEqual(TEXT("Widget displays item count"), DisplayedItems[0].Count, 2);
		TestEqual(TEXT("Widget preserves item rarity"), DisplayedItems[0].Rarity, EPRItemRarity::Uncommon);
	}

	TestTrue(TEXT("Inventory stacks second UI test item"), Inventory->AddItem(MakeInventoryUITestItem(TEXT("HealthInjector"), 3, EPRItemRarity::Uncommon)));
	DisplayedItems = Widget->GetDisplayedItems();
	TestEqual(TEXT("Stacked item remains a single displayed slot"), DisplayedItems.Num(), 1);
	if (DisplayedItems.Num() == 1)
	{
		TestEqual(TEXT("Widget displays stacked item count"), DisplayedItems[0].Count, 5);
	}

	Widget->BindInventory(nullptr);
	TestNull(TEXT("Widget can clear bound inventory"), Widget->GetBoundInventory());
	TestTrue(TEXT("Cleared widget displays empty inventory"), Widget->IsInventoryEmpty());
	TestTrue(TEXT("Original inventory can still change after widget unbinds"), Inventory->AddItem(MakeInventoryUITestItem(TEXT("ShieldPack"), 1)));
	TestEqual(TEXT("Unbound widget is not updated by old inventory"), Widget->GetDisplayedItems().Num(), 0);

	APRPlayerState* ResourcePlayerState = World->SpawnActor<APRPlayerState>();
	UPRInventoryWidget* ResourceWidget = NewObject<UPRInventoryWidget>(GetTransientPackage(), InventoryWidgetClass);
	TestNotNull(TEXT("Can spawn player state for ship resource UI"), ResourcePlayerState);
	TestNotNull(TEXT("Can instantiate ship resource UI widget"), ResourceWidget);
	if (ResourcePlayerState && ResourceWidget)
	{
		TestTrue(TEXT("Can seed extracted ship resource for inventory UI"), ResourcePlayerState->AddShipResource(TEXT("EnergyCrystal"), 5));
		ResourceWidget->BindInventory(ResourcePlayerState->GetInventoryComponent());
		TestTrue(
			TEXT("Inventory UI shows extracted ship resources even when the carried bag is empty"),
			ResourceWidget->GetShipResourceSummaryText().ToString().Contains(TEXT("EnergyCrystal x5")));
		TestTrue(TEXT("Ship resource display does not create carried inventory slots"), ResourceWidget->IsInventoryEmpty());
		TestTrue(
			TEXT("Inventory UI observes direct equipment changes for fixed-slot summaries"),
			ResourcePlayerState->GetEquipmentComponent()->OnEquipmentChanged.IsBound());
	}

	UPRInventoryComponent* DataInventory = NewObject<UPRInventoryComponent>(GetTransientPackage());
	UPRInventoryWidget* DataWidget = NewObject<UPRInventoryWidget>(GetTransientPackage(), InventoryWidgetClass);
	UPRItemDataAsset* HealthInjectorData = NewObject<UPRItemDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Can instantiate data-driven UI inventory"), DataInventory);
	TestNotNull(TEXT("Can instantiate data-driven UI widget"), DataWidget);
	TestNotNull(TEXT("Can instantiate UI item data"), HealthInjectorData);
	if (!DataInventory || !DataWidget || !HealthInjectorData)
	{
		return false;
	}

	HealthInjectorData->ItemId = TEXT("HealthInjector");
	HealthInjectorData->DisplayName = FText::FromString(TEXT("Field Medic Injector"));
	HealthInjectorData->Description = FText::FromString(TEXT("Restores health during early inventory tests."));
	TestTrue(TEXT("Can register UI item data"), SetInventoryUITestItemDataAssets(*this, DataInventory, { HealthInjectorData }));
	DataWidget->BindInventory(DataInventory);
	TestTrue(TEXT("Can add data-backed UI test item"), DataInventory->AddItem(MakeInventoryUITestItem(TEXT("HealthInjector"), 1)));
	const TArray<FPRItemInstance> DataDisplayedItems = DataWidget->GetDisplayedItems();
	TestEqual(TEXT("Data-backed UI inventory displays one item"), DataDisplayedItems.Num(), 1);
	if (DataDisplayedItems.Num() == 1)
	{
		TestEqual(
			TEXT("Inventory UI uses item data display name instead of raw ItemId"),
			DataWidget->GetItemDisplayName(DataDisplayedItems[0]).ToString(),
			FString(TEXT("Field Medic Injector")));
		TestTrue(
			TEXT("Inventory UI tooltip includes item data description"),
			DataWidget->GetItemTooltipText(DataDisplayedItems[0]).ToString().Contains(TEXT("Restores health")));
	}

	return true;
}

#endif
