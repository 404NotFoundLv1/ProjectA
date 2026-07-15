#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "GameplayEffect.h"
#include "Items/PRWeaponDataAsset.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRItemDataTest, "ProjectRift.Items.Data", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool TestEnumHasValue(FAutomationTestBase& Test, const UEnum* Enum, const TCHAR* ValueName)
{
	if (!Enum)
	{
		Test.AddError(FString::Printf(TEXT("Enum is missing while checking %s"), ValueName));
		return false;
	}

	const int64 Value = Enum->GetValueByNameString(ValueName);
	Test.TestTrue(FString::Printf(TEXT("%s enum value exists"), ValueName), Value != INDEX_NONE);
	return Value != INDEX_NONE;
}

template <typename PropertyType>
PropertyType* TestStructProperty(FAutomationTestBase& Test, UStruct* Struct, const TCHAR* PropertyName)
{
	PropertyType* Property = Struct ? FindFProperty<PropertyType>(Struct, PropertyName) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s property exists"), PropertyName), Property);
	return Property;
}

void SetObjectItemId(UObject* Object, const FName ItemId)
{
	if (FNameProperty* ItemIdProperty = Object ? FindFProperty<FNameProperty>(Object->GetClass(), TEXT("ItemId")) : nullptr)
	{
		ItemIdProperty->SetPropertyValue_InContainer(Object, ItemId);
	}
}

bool TestItemDataLookup(FAutomationTestBase& Test, UClass* ItemDataClass, UClass* LibraryClass)
{
	if (!ItemDataClass || !LibraryClass)
	{
		Test.AddError(TEXT("Item data lookup cannot run without item data and library classes"));
		return false;
	}

	UFunction* FindFunction = LibraryClass->FindFunctionByName(TEXT("FindItemDataById"));
	Test.TestNotNull(TEXT("UPRItemDataLibrary exposes FindItemDataById"), FindFunction);
	if (!FindFunction)
	{
		return false;
	}

	UObject* HealthInjector = NewObject<UObject>(GetTransientPackage(), ItemDataClass);
	UObject* ShieldPack = NewObject<UObject>(GetTransientPackage(), ItemDataClass);
	SetObjectItemId(HealthInjector, TEXT("HealthInjector"));
	SetObjectItemId(ShieldPack, TEXT("ShieldPack"));

	FStructOnScope Params(FindFunction);
	void* ParamsMemory = Params.GetStructMemory();

	FArrayProperty* AssetsProperty = FindFProperty<FArrayProperty>(FindFunction, TEXT("ItemDataAssets"));
	FObjectProperty* InnerObjectProperty = AssetsProperty ? CastField<FObjectProperty>(AssetsProperty->Inner) : nullptr;
	Test.TestNotNull(TEXT("FindItemDataById accepts an array of item data assets"), AssetsProperty);
	Test.TestNotNull(TEXT("FindItemDataById array stores object references"), InnerObjectProperty);
	if (!AssetsProperty || !InnerObjectProperty)
	{
		return false;
	}

	FScriptArrayHelper AssetsHelper(AssetsProperty, AssetsProperty->ContainerPtrToValuePtr<void>(ParamsMemory));
	AssetsHelper.AddValue();
	InnerObjectProperty->SetObjectPropertyValue(AssetsHelper.GetRawPtr(0), HealthInjector);
	AssetsHelper.AddValue();
	InnerObjectProperty->SetObjectPropertyValue(AssetsHelper.GetRawPtr(1), ShieldPack);

	FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(FindFunction, TEXT("ItemId"));
	Test.TestNotNull(TEXT("FindItemDataById accepts an ItemId"), ItemIdProperty);
	if (!ItemIdProperty)
	{
		return false;
	}
	ItemIdProperty->SetPropertyValue_InContainer(ParamsMemory, TEXT("ShieldPack"));

	UObject* LibraryDefaultObject = LibraryClass->GetDefaultObject();
	LibraryDefaultObject->ProcessEvent(FindFunction, ParamsMemory);

	FObjectProperty* ReturnProperty = FindFProperty<FObjectProperty>(FindFunction, TEXT("ReturnValue"));
	Test.TestNotNull(TEXT("FindItemDataById returns an item data asset"), ReturnProperty);
	UObject* FoundItemData = ReturnProperty ? ReturnProperty->GetObjectPropertyValue_InContainer(ParamsMemory) : nullptr;
	Test.TestEqual(TEXT("FindItemDataById returns the matching asset"), FoundItemData, ShieldPack);

	return FoundItemData == ShieldPack;
}

bool TestItemDataAsset(
	FAutomationTestBase& Test,
	UClass* ItemDataClass,
	const TCHAR* AssetPath,
	const FName ExpectedItemId,
	const TCHAR* ExpectedItemType,
	const int32 ExpectedMaxStackCount,
	const bool bExpectedCanUse)
{
	UObject* ItemAsset = ItemDataClass ? StaticLoadObject(ItemDataClass, nullptr, AssetPath) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s test item asset exists"), AssetPath), ItemAsset);
	if (!ItemAsset)
	{
		return false;
	}

	const FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(ItemAsset->GetClass(), TEXT("ItemId"));
	const FName ActualItemId = ItemIdProperty ? ItemIdProperty->GetPropertyValue_InContainer(ItemAsset) : NAME_None;
	Test.TestEqual(FString::Printf(TEXT("%s ItemId"), AssetPath), ActualItemId, ExpectedItemId);

	const FEnumProperty* ItemTypeProperty = FindFProperty<FEnumProperty>(ItemAsset->GetClass(), TEXT("ItemType"));
	const int64 ActualItemType = ItemTypeProperty
		? ItemTypeProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ItemTypeProperty->ContainerPtrToValuePtr<void>(ItemAsset))
		: INDEX_NONE;
	const int64 ExpectedItemTypeValue = ItemTypeProperty ? ItemTypeProperty->GetEnum()->GetValueByNameString(ExpectedItemType) : INDEX_NONE;
	Test.TestEqual(FString::Printf(TEXT("%s ItemType"), AssetPath), ActualItemType, ExpectedItemTypeValue);

	const FIntProperty* MaxStackCountProperty = FindFProperty<FIntProperty>(ItemAsset->GetClass(), TEXT("MaxStackCount"));
	const int32 ActualMaxStackCount = MaxStackCountProperty ? MaxStackCountProperty->GetPropertyValue_InContainer(ItemAsset) : INDEX_NONE;
	Test.TestEqual(FString::Printf(TEXT("%s MaxStackCount"), AssetPath), ActualMaxStackCount, ExpectedMaxStackCount);

	const FBoolProperty* CanUseProperty = FindFProperty<FBoolProperty>(ItemAsset->GetClass(), TEXT("bCanUse"));
	const bool bActualCanUse = CanUseProperty && CanUseProperty->GetPropertyValue_InContainer(ItemAsset);
	Test.TestEqual(FString::Printf(TEXT("%s bCanUse"), AssetPath), bActualCanUse, bExpectedCanUse);

	return ActualItemId == ExpectedItemId
		&& ActualItemType == ExpectedItemTypeValue
		&& ActualMaxStackCount == ExpectedMaxStackCount
		&& bActualCanUse == bExpectedCanUse;
}
}

bool FPRItemDataTest::RunTest(const FString& Parameters)
{
	UEnum* ItemTypeEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRItemType"));
	TestNotNull(TEXT("EPRItemType enum exists"), ItemTypeEnum);
	TestEnumHasValue(*this, ItemTypeEnum, TEXT("Consumable"));
	TestEnumHasValue(*this, ItemTypeEnum, TEXT("Equipment"));
	TestEnumHasValue(*this, ItemTypeEnum, TEXT("Material"));
	TestEnumHasValue(*this, ItemTypeEnum, TEXT("QuestItem"));
	TestEnumHasValue(*this, ItemTypeEnum, TEXT("Ammunition"));

	UEnum* ItemRarityEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRItemRarity"));
	TestNotNull(TEXT("EPRItemRarity enum exists"), ItemRarityEnum);
	TestEnumHasValue(*this, ItemRarityEnum, TEXT("Common"));
	TestEnumHasValue(*this, ItemRarityEnum, TEXT("Uncommon"));
	TestEnumHasValue(*this, ItemRarityEnum, TEXT("Rare"));

	UScriptStruct* ItemInstanceStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemInstance"));
	TestNotNull(TEXT("FPRItemInstance struct exists"), ItemInstanceStruct);
	TestStructProperty<FNameProperty>(*this, ItemInstanceStruct, TEXT("ItemId"));
	TestStructProperty<FIntProperty>(*this, ItemInstanceStruct, TEXT("Count"));
	TestStructProperty<FIntProperty>(*this, ItemInstanceStruct, TEXT("Level"));
	TestStructProperty<FEnumProperty>(*this, ItemInstanceStruct, TEXT("Rarity"));
	TestStructProperty<FFloatProperty>(*this, ItemInstanceStruct, TEXT("Durability"));
	FArrayProperty* AffixesProperty = TestStructProperty<FArrayProperty>(*this, ItemInstanceStruct, TEXT("Affixes"));
	TestTrue(TEXT("FPRItemInstance Affixes stores names"), AffixesProperty && AffixesProperty->Inner->IsA<FNameProperty>());

	UClass* ItemDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRItemDataAsset"));
	TestNotNull(TEXT("UPRItemDataAsset class exists"), ItemDataClass);
	TestTrue(TEXT("UPRItemDataAsset derives from UDataAsset"), ItemDataClass && ItemDataClass->IsChildOf(UDataAsset::StaticClass()));
	TestStructProperty<FNameProperty>(*this, ItemDataClass, TEXT("ItemId"));
	TestStructProperty<FTextProperty>(*this, ItemDataClass, TEXT("DisplayName"));
	TestStructProperty<FTextProperty>(*this, ItemDataClass, TEXT("Description"));
	FObjectPropertyBase* IconProperty = TestStructProperty<FObjectPropertyBase>(*this, ItemDataClass, TEXT("Icon"));
	TestTrue(TEXT("UPRItemDataAsset Icon stores UTexture2D"), IconProperty && IconProperty->PropertyClass->IsChildOf(UTexture2D::StaticClass()));
	TestStructProperty<FEnumProperty>(*this, ItemDataClass, TEXT("ItemType"));
	TestStructProperty<FIntProperty>(*this, ItemDataClass, TEXT("MaxStackCount"));
	TestStructProperty<FBoolProperty>(*this, ItemDataClass, TEXT("bCanUse"));
	TestStructProperty<FBoolProperty>(*this, ItemDataClass, TEXT("bCanEquip"));
	TestStructProperty<FBoolProperty>(*this, ItemDataClass, TEXT("bCanDrop"));
	FClassProperty* UseEffectProperty = TestStructProperty<FClassProperty>(*this, ItemDataClass, TEXT("UseEffect"));
	TestTrue(TEXT("UPRItemDataAsset UseEffect stores GameplayEffect classes"), UseEffectProperty && UseEffectProperty->MetaClass->IsChildOf(UGameplayEffect::StaticClass()));

	UClass* ItemDataLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRItemDataLibrary"));
	TestNotNull(TEXT("UPRItemDataLibrary class exists"), ItemDataLibraryClass);
	TestItemDataLookup(*this, ItemDataClass, ItemDataLibraryClass);

	TestItemDataAsset(
		*this,
		ItemDataClass,
		TEXT("/Game/ProjectRift/Items/DA_HealthInjector.DA_HealthInjector"),
		TEXT("HealthInjector"),
		TEXT("Consumable"),
		5,
		true);
	TestItemDataAsset(
		*this,
		ItemDataClass,
		TEXT("/Game/ProjectRift/Items/DA_ShieldPack.DA_ShieldPack"),
		TEXT("ShieldPack"),
		TEXT("Consumable"),
		5,
		true);
	TestItemDataAsset(
		*this,
		ItemDataClass,
		TEXT("/Game/ProjectRift/Items/DA_EnergyCrystal.DA_EnergyCrystal"),
		TEXT("EnergyCrystal"),
		TEXT("Material"),
		99,
		false);
	TestItemDataAsset(
		*this,
		ItemDataClass,
		TEXT("/Game/ProjectRift/Items/DA_CommonChip.DA_CommonChip"),
		TEXT("CommonChip"),
		TEXT("Material"),
		99,
		false);

	UPRWeaponDataAsset* Rifle = LoadObject<UPRWeaponDataAsset>(
		nullptr,
		TEXT("/Game/ProjectRift/Items/DA_TestRifle.DA_TestRifle"));
	TestNotNull(TEXT("Test rifle data asset exists"), Rifle);
	if (Rifle)
	{
		TestEqual(TEXT("Test rifle ItemId"), Rifle->ItemId, FName(TEXT("TestRifle")));
		TestEqual(TEXT("Test rifle item type"), Rifle->ItemType, EPRItemType::Equipment);
		TestTrue(TEXT("Test rifle can equip"), Rifle->bCanEquip);
		TestFalse(TEXT("Test rifle cannot drop"), Rifle->bCanDrop);
		TestEqual(TEXT("Test rifle ammo ItemId"), Rifle->AmmoItemId, FName(TEXT("RifleAmmo")));
		TestEqual(TEXT("Test rifle magazine capacity"), Rifle->MagazineCapacity, 12);
		TestEqual(TEXT("Test rifle initial reserve"), Rifle->InitialReserveAmmo, 48);
		TestTrue(TEXT("Test rifle damage is 12"), FMath::IsNearlyEqual(Rifle->BaseDamage, 12.0f));
		TestTrue(TEXT("Test rifle range is 12000"), FMath::IsNearlyEqual(Rifle->Range, 12000.0f));
		TestTrue(TEXT("Test rifle interval is 0.18"), FMath::IsNearlyEqual(Rifle->MinFireInterval, 0.18f));
		TestTrue(TEXT("Test rifle hip spread is 2.5"), FMath::IsNearlyEqual(Rifle->HipSpreadDegrees, 2.5f));
		TestTrue(TEXT("Test rifle ADS spread is 0.35"), FMath::IsNearlyEqual(Rifle->AimSpreadDegrees, 0.35f));
		TestTrue(TEXT("Test rifle reload is 1.5"), FMath::IsNearlyEqual(Rifle->ReloadDuration, 1.5f));
		TestTrue(TEXT("Test rifle ADS arm is 250"), FMath::IsNearlyEqual(Rifle->AimArmLength, 250.0f));
		TestTrue(TEXT("Test rifle ADS FOV is 70"), FMath::IsNearlyEqual(Rifle->AimFieldOfView, 70.0f));
		TestEqual(TEXT("Test rifle socket"), Rifle->AttachSocketName, FName(TEXT("HandGrip_R")));
	}

	UPRItemDataAsset* RifleAmmo = LoadObject<UPRItemDataAsset>(
		nullptr,
		TEXT("/Game/ProjectRift/Items/DA_RifleAmmo.DA_RifleAmmo"));
	TestNotNull(TEXT("Rifle ammo data asset exists"), RifleAmmo);
	if (RifleAmmo)
	{
		TestEqual(TEXT("Rifle ammo ItemId"), RifleAmmo->ItemId, FName(TEXT("RifleAmmo")));
		TestEqual(TEXT("Rifle ammo item type"), RifleAmmo->ItemType, EPRItemType::Ammunition);
		TestEqual(TEXT("Rifle ammo stack limit"), RifleAmmo->MaxStackCount, 120);
		TestFalse(TEXT("Rifle ammo cannot use"), RifleAmmo->bCanUse);
		TestFalse(TEXT("Rifle ammo cannot equip"), RifleAmmo->bCanEquip);
	}

	return true;
}

#endif
