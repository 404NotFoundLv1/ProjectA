#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRWeaponDataAsset.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/PRWeaponComponent.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRWeaponRuntimeTest,
	"ProjectRift.Weapons.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FWeaponTestData
{
	TStrongObjectPtr<UPRWeaponDataAsset> Weapon;
	TStrongObjectPtr<UPRItemDataAsset> Ammo;
	TStrongObjectPtr<UPRItemDataAsset> Legacy;
};

FWeaponTestData MakeWeaponTestData()
{
	FWeaponTestData Data;
	Data.Weapon.Reset(NewObject<UPRWeaponDataAsset>(GetTransientPackage()));
	Data.Weapon->ItemId = TEXT("TestRifle");
	Data.Weapon->DisplayName = FText::FromString(TEXT("Test Rifle"));
	Data.Weapon->AmmoItemId = TEXT("RifleAmmo");
	Data.Weapon->MagazineCapacity = 12;
	Data.Weapon->InitialReserveAmmo = 48;
	Data.Weapon->MinFireInterval = 100.0f;
	Data.Weapon->ReloadDuration = 0.0f;
	Data.Weapon->HipSpreadDegrees = 0.0f;
	Data.Weapon->AimSpreadDegrees = 0.0f;

	Data.Ammo.Reset(NewObject<UPRItemDataAsset>(GetTransientPackage()));
	Data.Ammo->ItemId = TEXT("RifleAmmo");
	Data.Ammo->ItemType = EPRItemType::Ammunition;
	Data.Ammo->MaxStackCount = 120;
	Data.Ammo->bCanUse = false;
	Data.Ammo->bCanEquip = false;

	Data.Legacy.Reset(NewObject<UPRItemDataAsset>(GetTransientPackage()));
	Data.Legacy->ItemId = TEXT("LegacyBlade");
	Data.Legacy->ItemType = EPRItemType::Equipment;
	Data.Legacy->MaxStackCount = 1;
	return Data;
}

bool PossessWeaponTestCharacter(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
{
	APlayerController* Controller = World ? World->SpawnActor<APlayerController>() : nullptr;
	if (!Controller || !PlayerState || !Character)
	{
		return false;
	}
	Controller->SetPlayerState(PlayerState);
	Controller->Possess(Character);
	return Character->IsAbilitySystemInitialized();
}

void RegisterWeaponTestData(UPRInventoryComponent* Inventory, const FWeaponTestData& Data)
{
	Inventory->SetItemDataAssets({ Data.Weapon.Get(), Data.Ammo.Get(), Data.Legacy.Get() });
}

int32 CountItem(const TArray<FPRItemInstance>& Items, const FName ItemId)
{
	int32 Count = 0;
	for (const FPRItemInstance& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			Count += Item.Count;
		}
	}
	return Count;
}
}

bool FPRWeaponRuntimeTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Weapon runtime test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Weapon runtime test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	const FWeaponTestData Data = MakeWeaponTestData();
	TStrongObjectPtr<APRPlayerState> PlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> Character{ World->SpawnActor<APRCharacter>() };
	TestNotNull(TEXT("Weapon PlayerState spawns"), PlayerState.Get());
	TestNotNull(TEXT("Weapon Character spawns"), Character.Get());
	if (!PlayerState || !Character)
	{
		return false;
	}
	UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent();
	RegisterWeaponTestData(Inventory, Data);
	TestTrue(TEXT("Weapon character initializes ASC and avatar"), PossessWeaponTestCharacter(World, PlayerState.Get(), Character.Get()));

	FPRItemInstance Rifle;
	Rifle.ItemId = TEXT("TestRifle");
	Rifle.Count = 1;
	FPRItemInstance Ammo;
	Ammo.ItemId = TEXT("RifleAmmo");
	Ammo.Count = 60;
	TestTrue(TEXT("Test rifle enters backpack"), Inventory->AddItem(Rifle));
	TestTrue(TEXT("Test ammo enters backpack"), Inventory->AddItem(Ammo));
	TestTrue(TEXT("Rifle equips atomically"), Weapon->EquipWeaponFromInventory(TEXT("TestRifle")));
	TestEqual(TEXT("Equipping fills a 12-round magazine"), Weapon->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Equipping leaves 48 reserve rounds"), Weapon->GetReserveAmmo(), 48);
	TestEqual(TEXT("Equipped rifle leaves the backpack"), Inventory->GetItemCount(TEXT("TestRifle")), 0);

	Character->DoPrimaryAttack();
	TestEqual(TEXT("Accepted shot consumes one round"), Weapon->GetMagazineAmmo(), 11);
	Character->DoPrimaryAttack();
	TestEqual(TEXT("Rate-limited shot consumes nothing"), Weapon->GetMagazineAmmo(), 11);

	Data.Weapon->MinFireInterval = 0.0f;
	TestTrue(TEXT("Partial magazine starts reload"), Weapon->StartReload());
	TestFalse(TEXT("Zero-duration reload completes immediately"), Weapon->IsReloading());
	TestEqual(TEXT("Reload fills only the missing round"), Weapon->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Reload consumes one reserve round"), Weapon->GetReserveAmmo(), 47);

	TestEqual(TEXT("Shot before cancellation is accepted"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	Data.Weapon->ReloadDuration = 1.5f;
	TestTrue(TEXT("Second partial reload starts"), Weapon->StartReload());
	Weapon->CancelReload();
	TestFalse(TEXT("Canceled reload remains inactive"), Weapon->IsReloading());
	TestEqual(TEXT("Canceled reload preserves magazine"), Weapon->GetMagazineAmmo(), 11);
	TestEqual(TEXT("Canceled reload preserves reserve"), Weapon->GetReserveAmmo(), 47);

	TArray<FPRItemInstance> PersistentBackpack;
	FString Diagnostic;
	TestTrue(TEXT("Persistent backpack merges loaded magazine"), Weapon->BuildPersistentBackpack(PersistentBackpack, Diagnostic));
	TestEqual(TEXT("Persistent ammo stores exact total"), CountItem(PersistentBackpack, TEXT("RifleAmmo")), 58);

	TestTrue(TEXT("Weapon can be unequipped"), Weapon->UnequipWeapon());
	TestEqual(TEXT("Unequip clears magazine"), Weapon->GetMagazineAmmo(), 0);
	TestEqual(TEXT("Unequip returns all remaining ammo"), Inventory->GetItemCount(TEXT("RifleAmmo")), 58);
	TestEqual(TEXT("Unequip returns rifle to backpack"), Inventory->GetItemCount(TEXT("TestRifle")), 1);
	TestTrue(TEXT("Returned rifle can be re-equipped"), Weapon->EquipWeaponFromInventory(TEXT("TestRifle")));
	TestEqual(TEXT("Re-equip repartitions ammo into full magazine"), Weapon->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Re-equip preserves total with 46 reserve"), Weapon->GetReserveAmmo(), 46);

	const int32 AmmoBeforeInvalidInput = Weapon->GetMagazineAmmo();
	Data.Weapon->BaseDamage = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Non-finite weapon damage fails closed"), Weapon->TryFire(), EPRWeaponFireResult::Invalid);
	TestEqual(TEXT("Non-finite weapon damage consumes no ammo"), Weapon->GetMagazineAmmo(), AmmoBeforeInvalidInput);
	Data.Weapon->BaseDamage = 12.0f;
	Data.Weapon->Range = 0.0f;
	TestEqual(TEXT("Non-positive weapon range fails closed"), Weapon->TryFire(), EPRWeaponFireResult::Invalid);
	TestEqual(TEXT("Non-positive weapon range consumes no ammo"), Weapon->GetMagazineAmmo(), AmmoBeforeInvalidInput);
	Data.Weapon->Range = 12000.0f;

	UPRAbilitySystemComponent* PlayerASC = PlayerState->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("Weapon runtime PlayerState exposes its ASC"), PlayerASC);
	if (!PlayerASC)
	{
		return false;
	}
	TestEqual(TEXT("Accepted shot prepares a partial magazine for stagger blocking"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	const int32 MagazineBeforeStagger = Weapon->GetMagazineAmmo();
	const int32 ReserveBeforeStagger = Weapon->GetReserveAmmo();
	PlayerASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_HitStaggered);
	TestEqual(TEXT("State.HitStaggered blocks accepted weapon fire"), Weapon->TryFire(), EPRWeaponFireResult::Inactive);
	TestEqual(TEXT("Stagger-blocked fire consumes no magazine ammo"), Weapon->GetMagazineAmmo(), MagazineBeforeStagger);
	Weapon->SetAiming(true);
	TestFalse(TEXT("State.HitStaggered blocks entering ADS"), Weapon->IsAiming());
	TestFalse(TEXT("State.HitStaggered blocks starting reload"), Weapon->StartReload());
	TestFalse(TEXT("Stagger-blocked reload never enters the reloading state"), Weapon->IsReloading());
	TestEqual(TEXT("Stagger-blocked reload preserves magazine ammo"), Weapon->GetMagazineAmmo(), MagazineBeforeStagger);
	TestEqual(TEXT("Stagger-blocked reload preserves reserve ammo"), Weapon->GetReserveAmmo(), ReserveBeforeStagger);
	PlayerASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_HitStaggered);
	// Keep the pre-existing downed-state regression independent while this new behavior is red.
	Weapon->CancelReload();
	Weapon->SetAiming(false);

	TestEqual(TEXT("Shot before downed state is accepted"), Weapon->TryFire(), EPRWeaponFireResult::FiredMiss);
	TestTrue(TEXT("Reload starts before downed state"), Weapon->StartReload());
	TestTrue(TEXT("Character enters downed state"), Character->EnterDownedState());
	TestFalse(TEXT("Downed state cancels reload"), Weapon->IsReloading());
	TestFalse(TEXT("Downed state cancels aim"), Weapon->IsAiming());
	TestEqual(TEXT("Canceled downed reload does not consume reserve"), Weapon->GetReserveAmmo(), 46);

	TStrongObjectPtr<APRPlayerState> StarterState{ World->SpawnActor<APRPlayerState>() };
	RegisterWeaponTestData(StarterState->GetInventoryComponent(), Data);
	UPRWeaponComponent* StarterWeapon = StarterState->GetWeaponComponent();
	Diagnostic.Reset();
	TestTrue(TEXT("Empty profile receives starter rifle"), StarterWeapon->EnsureStarterWeapon(TEXT("TestRifle"), Diagnostic));
	TestEqual(TEXT("Starter rifle begins with full magazine"), StarterWeapon->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Starter rifle begins with 48 reserve"), StarterWeapon->GetReserveAmmo(), 48);
	TestTrue(TEXT("Starter grant is idempotent"), StarterWeapon->EnsureStarterWeapon(TEXT("TestRifle"), Diagnostic));
	TestEqual(TEXT("Idempotent grant does not duplicate reserve"), StarterWeapon->GetReserveAmmo(), 48);

	TStrongObjectPtr<APRPlayerState> LegacyState{ World->SpawnActor<APRPlayerState>() };
	RegisterWeaponTestData(LegacyState->GetInventoryComponent(), Data);
	FPRItemInstance LegacyItem;
	LegacyItem.ItemId = TEXT("LegacyBlade");
	LegacyItem.Count = 1;
	TArray<FPRProfileEquipmentEntry> LegacyEquipment{ FPRProfileEquipmentEntry(TEXT("Slot.Primary"), LegacyItem) };
	Diagnostic.Reset();
	TestTrue(TEXT("Unknown legacy primary equipment applies without data loss"), LegacyState->GetWeaponComponent()->ReplaceEquipmentEntries(LegacyEquipment, Diagnostic));
	TestFalse(TEXT("Unknown legacy equipment is not treated as a firearm"), LegacyState->GetWeaponComponent()->GetEquippedWeapon().IsValid());
	TestEqual(TEXT("Unknown legacy primary entry is retained"), LegacyState->GetWeaponComponent()->GetEquipmentEntries()[0].Item.ItemId, FName(TEXT("LegacyBlade")));
	TestTrue(TEXT("Starter rifle is placed in backpack beside legacy primary"), LegacyState->GetWeaponComponent()->EnsureStarterWeapon(TEXT("TestRifle"), Diagnostic));
	TestEqual(TEXT("Legacy primary remains intact after starter grant"), LegacyState->GetWeaponComponent()->GetEquipmentEntries()[0].Item.ItemId, FName(TEXT("LegacyBlade")));
	TestEqual(TEXT("Starter rifle remains available in backpack"), LegacyState->GetInventoryComponent()->GetItemCount(TEXT("TestRifle")), 1);

	return true;
}

#endif
