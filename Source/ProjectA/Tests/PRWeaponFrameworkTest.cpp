#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRWeaponFrameworkContractTest,
	"ProjectRift.Weapons.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
template <typename PropertyType>
PropertyType* RequireProperty(FAutomationTestBase& Test, UStruct* Owner, const TCHAR* Name)
{
	PropertyType* Property = Owner ? FindFProperty<PropertyType>(Owner, Name) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exists"), Name), Property);
	return Property;
}

void RequireFunction(FAutomationTestBase& Test, UClass* Owner, const TCHAR* Name)
{
	Test.TestNotNull(FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Owner), Name), Owner ? Owner->FindFunctionByName(Name) : nullptr);
}
}

bool FPRWeaponFrameworkContractTest::RunTest(const FString& Parameters)
{
	UEnum* ItemTypeEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRItemType"));
	TestNotNull(TEXT("EPRItemType exists"), ItemTypeEnum);
	TestTrue(TEXT("EPRItemType appends Ammunition"), ItemTypeEnum && ItemTypeEnum->GetValueByNameString(TEXT("Ammunition")) != INDEX_NONE);

	UClass* ItemDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRItemDataAsset"));
	TestNotNull(TEXT("UPRItemDataAsset exists"), ItemDataClass);
	RequireProperty<FBoolProperty>(*this, ItemDataClass, TEXT("bCanDrop"));

	UClass* WeaponDataClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponDataAsset"));
	TestNotNull(TEXT("UPRWeaponDataAsset exists"), WeaponDataClass);
	TestTrue(TEXT("UPRWeaponDataAsset derives from UPRItemDataAsset"), WeaponDataClass && ItemDataClass && WeaponDataClass->IsChildOf(ItemDataClass));
	RequireProperty<FNameProperty>(*this, WeaponDataClass, TEXT("AmmoItemId"));
	RequireProperty<FIntProperty>(*this, WeaponDataClass, TEXT("MagazineCapacity"));
	RequireProperty<FIntProperty>(*this, WeaponDataClass, TEXT("InitialReserveAmmo"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("BaseDamage"));
	RequireProperty<FStructProperty>(*this, WeaponDataClass, TEXT("DamageType"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("Range"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("MinFireInterval"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("HipSpreadDegrees"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("AimSpreadDegrees"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("ReloadDuration"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("AimArmLength"));
	RequireProperty<FFloatProperty>(*this, WeaponDataClass, TEXT("AimFieldOfView"));
	RequireProperty<FNameProperty>(*this, WeaponDataClass, TEXT("AttachSocketName"));
	RequireProperty<FClassProperty>(*this, WeaponDataClass, TEXT("WeaponActorClass"));

	UClass* WeaponComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponComponent"));
	TestNotNull(TEXT("UPRWeaponComponent exists"), WeaponComponentClass);
	TestTrue(TEXT("UPRWeaponComponent derives from UActorComponent"), WeaponComponentClass && WeaponComponentClass->IsChildOf(UActorComponent::StaticClass()));
	if (const UActorComponent* ComponentCDO = WeaponComponentClass ? Cast<UActorComponent>(WeaponComponentClass->GetDefaultObject()) : nullptr)
	{
		TestTrue(TEXT("UPRWeaponComponent replicates"), ComponentCDO->GetIsReplicated());
	}
	RequireFunction(*this, WeaponComponentClass, TEXT("GetEquippedWeapon"));
	RequireFunction(*this, WeaponComponentClass, TEXT("GetMagazineAmmo"));
	RequireFunction(*this, WeaponComponentClass, TEXT("GetReserveAmmo"));
	RequireFunction(*this, WeaponComponentClass, TEXT("IsAiming"));
	RequireFunction(*this, WeaponComponentClass, TEXT("IsReloading"));
	RequireFunction(*this, WeaponComponentClass, TEXT("EquipWeaponFromInventory"));
	RequireFunction(*this, WeaponComponentClass, TEXT("UnequipWeapon"));
	RequireFunction(*this, WeaponComponentClass, TEXT("TryFire"));
	RequireFunction(*this, WeaponComponentClass, TEXT("SetAiming"));
	RequireFunction(*this, WeaponComponentClass, TEXT("StartReload"));
	RequireFunction(*this, WeaponComponentClass, TEXT("CancelReload"));

	UClass* WeaponActorClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponActor"));
	TestNotNull(TEXT("APRWeaponActor exists"), WeaponActorClass);
	TestTrue(TEXT("APRWeaponActor derives from AActor"), WeaponActorClass && WeaponActorClass->IsChildOf(AActor::StaticClass()));
	RequireFunction(*this, WeaponActorClass, TEXT("GetMuzzleTransform"));

	UClass* WeaponHUDClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponHUDWidget"));
	TestNotNull(TEXT("UPRWeaponHUDWidget exists"), WeaponHUDClass);
	TestTrue(TEXT("UPRWeaponHUDWidget derives from UUserWidget"), WeaponHUDClass && WeaponHUDClass->IsChildOf(UUserWidget::StaticClass()));

	UClass* PlayerStateClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRPlayerState"));
	RequireProperty<FObjectPropertyBase>(*this, PlayerStateClass, TEXT("WeaponComponent"));
	RequireFunction(*this, PlayerStateClass, TEXT("GetWeaponComponent"));

	UScriptStruct* ProjectionStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRMultiplayerProfileProjection"));
	FArrayProperty* ProjectionEquipment = RequireProperty<FArrayProperty>(*this, ProjectionStruct, TEXT("Equipment"));
	TestTrue(TEXT("Multiplayer projection equipment stores profile equipment entries"),
		ProjectionEquipment && ProjectionEquipment->Inner && ProjectionEquipment->Inner->IsA<FStructProperty>());

	UClass* CharacterClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRCharacter"));
	RequireFunction(*this, CharacterClass, TEXT("DoAim"));
	RequireFunction(*this, CharacterClass, TEXT("DoAimReleased"));
	RequireFunction(*this, CharacterClass, TEXT("DoReload"));

	for (const TCHAR* TagName : { TEXT("State.Aiming"), TEXT("State.Reloading") })
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(TagName), false);
		TestTrue(*FString::Printf(TEXT("%s is registered"), TagName), Tag.IsValid());
	}

	return true;
}

#endif
