#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerController.h"
#include "UI/PRInventoryWidget.h"
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

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("APRPlayerController class exists for inventory UI"), PlayerControllerClass);
	TestNotNull(TEXT("Player controller exposes ToggleInventory"), PlayerControllerClass->FindFunctionByName(TEXT("ToggleInventory")));
	TestNotNull(TEXT("Player controller exposes ShowInventory"), PlayerControllerClass->FindFunctionByName(TEXT("ShowInventory")));
	TestNotNull(TEXT("Player controller exposes HideInventory"), PlayerControllerClass->FindFunctionByName(TEXT("HideInventory")));
	TestNotNull(TEXT("Player controller exposes RefreshInventoryDisplay"), PlayerControllerClass->FindFunctionByName(TEXT("RefreshInventoryDisplay")));
	TestNotNull(TEXT("Player controller exposes IsInventoryVisible"), PlayerControllerClass->FindFunctionByName(TEXT("IsInventoryVisible")));
	TestNotNull(TEXT("Player controller has configurable InventoryWidgetClass"), FindFProperty<FClassProperty>(PlayerControllerClass, TEXT("InventoryWidgetClass")));

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

	return true;
}

#endif
