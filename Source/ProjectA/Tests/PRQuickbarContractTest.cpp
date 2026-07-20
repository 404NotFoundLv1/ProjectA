#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Persistence/PRProfileSave.h"
#include "Persistence/PRProfileTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRQuickbarContractTest, "ProjectRift.Items.QuickbarContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindQuickbarContractClass(const TCHAR* ClassName)
{
	return FindObject<UClass>(nullptr, ClassName);
}

bool RequireQuickbarProperty(FAutomationTestBase& Test, UClass* Class, const FName PropertyName)
{
	const FProperty* Property = Class ? FindFProperty<FProperty>(Class, PropertyName) : nullptr;
	Test.TestNotNull(*FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Class), *PropertyName.ToString()), Property);
	return Property != nullptr;
}

bool RequireQuickbarFunction(FAutomationTestBase& Test, UClass* Class, const FName FunctionName)
{
	const UFunction* Function = Class ? Class->FindFunctionByName(FunctionName) : nullptr;
	Test.TestNotNull(*FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Class), *FunctionName.ToString()), Function);
	return Function != nullptr;
}
}

bool FPRQuickbarContractTest::RunTest(const FString& Parameters)
{
	UClass* QuickbarComponentClass = FindQuickbarContractClass(TEXT("/Script/ProjectA.PRQuickbarComponent"));
	TestNotNull(TEXT("Quickbar component class exists"), QuickbarComponentClass);
	RequireQuickbarFunction(*this, QuickbarComponentClass, TEXT("AssignSlot"));
	RequireQuickbarFunction(*this, QuickbarComponentClass, TEXT("ClearSlot"));
	RequireQuickbarFunction(*this, QuickbarComponentClass, TEXT("RequestUseSlot"));
	RequireQuickbarFunction(*this, QuickbarComponentClass, TEXT("CancelActiveUse"));
	RequireQuickbarFunction(*this, QuickbarComponentClass, TEXT("GetQuickSlots"));
	RequireQuickbarProperty(*this, QuickbarComponentClass, TEXT("QuickSlots"));
	RequireQuickbarProperty(*this, QuickbarComponentClass, TEXT("ActiveUse"));

	UClass* PlayerStateClass = FindQuickbarContractClass(TEXT("/Script/ProjectA.PRPlayerState"));
	TestNotNull(TEXT("ProjectRift player state class exists"), PlayerStateClass);
	RequireQuickbarFunction(*this, PlayerStateClass, TEXT("GetQuickbarComponent"));

	UClass* ItemDataClass = FindQuickbarContractClass(TEXT("/Script/ProjectA.PRItemDataAsset"));
	TestNotNull(TEXT("Item data class exists"), ItemDataClass);
	RequireQuickbarProperty(*this, ItemDataClass, TEXT("UseDefinition"));
	RequireQuickbarProperty(*this, ItemDataClass, TEXT("bCanCraft"));
	RequireQuickbarProperty(*this, ItemDataClass, TEXT("bAutoProcessAtMissionEnd"));

	UClass* QuickbarWidgetClass = FindQuickbarContractClass(TEXT("/Script/ProjectA.PRQuickbarHUDWidget"));
	TestNotNull(TEXT("Quickbar HUD widget class exists"), QuickbarWidgetClass);
	RequireQuickbarFunction(*this, QuickbarWidgetClass, TEXT("InitializeForController"));

	TestEqual(TEXT("Profile schema advances for crafting replay protection"), UPRProfileSave::LatestSaveVersion, 9);

	FPRProfileSnapshot Snapshot;
	FPRItemInstance Item;
	Item.ItemId = TEXT("HealthInjector");
	Item.Count = 1;
	Item.InstanceGuid = FGuid::NewGuid();
	Snapshot.BackpackItems.Add(Item);
	FPRQuickSlotReference ValidSlot;
	ValidSlot.SlotIndex = 0;
	ValidSlot.InstanceGuid = Item.InstanceGuid;
	FPRQuickSlotReference DuplicateSlot;
	DuplicateSlot.SlotIndex = 1;
	DuplicateSlot.InstanceGuid = Item.InstanceGuid;
	FPRQuickSlotReference MissingSlot;
	MissingSlot.SlotIndex = 2;
	MissingSlot.InstanceGuid = FGuid::NewGuid();
	Snapshot.QuickSlots = { ValidSlot, DuplicateSlot, MissingSlot };
	Snapshot.Normalize();
	TestEqual(TEXT("Profile normalization keeps exactly one resolvable quickbar GUID reference"), Snapshot.QuickSlots.Num(), 1);
	if (Snapshot.QuickSlots.Num() == 1)
	{
		TestEqual(TEXT("Profile normalization preserves the first valid slot binding"), Snapshot.QuickSlots[0].SlotIndex, 0);
		TestEqual(TEXT("Profile normalization preserves the resolved item GUID"), Snapshot.QuickSlots[0].InstanceGuid, Item.InstanceGuid);
	}
	return true;
}

#endif
