#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Player/PRPlayerState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREquipmentContractTest,
	"ProjectRift.Equipment.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
template <typename PropertyType>
PropertyType* RequireEquipmentProperty(FAutomationTestBase& Test, UStruct* Owner, const TCHAR* Name)
{
	PropertyType* Property = Owner ? FindFProperty<PropertyType>(Owner, Name) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exists"), Name), Property);
	return Property;
}
}

bool FPREquipmentContractTest::RunTest(const FString& Parameters)
{
	UEnum* SlotEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPREquipmentSlot"));
	TestNotNull(TEXT("Equipment slot enum is reflected"), SlotEnum);
	for (const TCHAR* SlotName : { TEXT("Weapon"), TEXT("Armor"), TEXT("Chip"), TEXT("Tool") })
	{
		TestTrue(
			FString::Printf(TEXT("Equipment slot %s exists"), SlotName),
			SlotEnum && SlotEnum->GetValueByNameString(SlotName) != INDEX_NONE);
	}

	UClass* ItemDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRItemDataAsset"));
	UClass* EquipmentDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREquipmentDataAsset"));
	UClass* WeaponDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponDataAsset"));
	TestNotNull(TEXT("Base item data class is reflected"), ItemDataClass);
	TestNotNull(TEXT("Equipment data class is reflected"), EquipmentDataClass);
	TestTrue(TEXT("Equipment data derives from item data"), EquipmentDataClass && ItemDataClass && EquipmentDataClass->IsChildOf(ItemDataClass));
	TestTrue(TEXT("Weapon data derives from equipment data"), WeaponDataClass && EquipmentDataClass && WeaponDataClass->IsChildOf(EquipmentDataClass));
	TestNotNull(TEXT("EquipmentSlot exists"), EquipmentDataClass ? FindFProperty<FProperty>(EquipmentDataClass, TEXT("EquipmentSlot")) : nullptr);
	RequireEquipmentProperty<FArrayProperty>(*this, EquipmentDataClass, TEXT("GrantedEffects"));
	RequireEquipmentProperty<FArrayProperty>(*this, EquipmentDataClass, TEXT("GrantedAbilities"));

	UClass* EquipmentComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREquipmentComponent"));
	TestNotNull(TEXT("Equipment component is reflected"), EquipmentComponentClass);
	UFunction* Accessor = APRPlayerState::StaticClass()->FindFunctionByName(TEXT("GetEquipmentComponent"));
	TestNotNull(TEXT("PlayerState exposes the equipment component"), Accessor);
	if (Accessor && EquipmentComponentClass)
	{
		const FObjectProperty* ReturnProperty = FindFProperty<FObjectProperty>(Accessor, TEXT("ReturnValue"));
		TestNotNull(TEXT("Equipment accessor returns an object"), ReturnProperty);
		if (ReturnProperty)
		{
			TestTrue(TEXT("Equipment accessor returns the equipment component type"), ReturnProperty->PropertyClass->IsChildOf(EquipmentComponentClass));
		}
	}

	UScriptStruct* RequestStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemTransactionRequest"));
	RequireEquipmentProperty<FNameProperty>(*this, RequestStruct, TEXT("SlotId"));
	UScriptStruct* AppearanceStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREquipmentAppearanceEntry"));
	TestNotNull(TEXT("Public equipment appearance entry is reflected"), AppearanceStruct);
	RequireEquipmentProperty<FNameProperty>(*this, AppearanceStruct, TEXT("SlotId"));
	RequireEquipmentProperty<FNameProperty>(*this, AppearanceStruct, TEXT("ItemId"));

	return true;
}

#endif
