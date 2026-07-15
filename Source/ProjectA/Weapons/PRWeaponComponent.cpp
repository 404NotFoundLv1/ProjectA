#include "Weapons/PRWeaponComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "Characters/PRCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PRGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRWeaponDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponActor.h"

namespace
{
bool AddItemToSnapshot(TArray<FPRItemInstance>& Items, const FPRItemInstance& Item, const int32 MaxStack)
{
	if (!Item.IsValid() || MaxStack <= 0)
	{
		return false;
	}

	int32 Remaining = Item.Count;
	for (FPRItemInstance& Existing : Items)
	{
		if (Existing.ItemId != Item.ItemId || Existing.Level != Item.Level || Existing.Rarity != Item.Rarity
			|| !FMath::IsNearlyEqual(Existing.Durability, Item.Durability) || Existing.Affixes != Item.Affixes)
		{
			continue;
		}
		const int32 Added = FMath::Min(Remaining, FMath::Max(0, MaxStack - Existing.Count));
		Existing.Count += Added;
		Remaining -= Added;
		if (Remaining <= 0)
		{
			return true;
		}
	}

	while (Remaining > 0)
	{
		FPRItemInstance& NewStack = Items.Add_GetRef(Item);
		NewStack.Count = FMath::Min(Remaining, MaxStack);
		Remaining -= NewStack.Count;
	}
	return true;
}

bool IsValidFireConfiguration(const UPRWeaponDataAsset* WeaponData)
{
	return WeaponData
		&& WeaponData->MagazineCapacity > 0
		&& FMath::IsFinite(WeaponData->BaseDamage)
		&& WeaponData->BaseDamage > 0.0f
		&& FMath::IsFinite(WeaponData->Range)
		&& WeaponData->Range > 0.0f
		&& FMath::IsFinite(WeaponData->MinFireInterval)
		&& WeaponData->MinFireInterval >= 0.0f
		&& FMath::IsFinite(WeaponData->HipSpreadDegrees)
		&& WeaponData->HipSpreadDegrees >= 0.0f
		&& WeaponData->HipSpreadDegrees <= 45.0f
		&& FMath::IsFinite(WeaponData->AimSpreadDegrees)
		&& WeaponData->AimSpreadDegrees >= 0.0f
		&& WeaponData->AimSpreadDegrees <= 45.0f;
}

FCollisionObjectQueryParams MakeWeaponTraceObjectQuery()
{
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	return ObjectQuery;
}
}

UPRWeaponComponent::UPRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPRWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelReloadAuthoritative();
	DestroyWeaponActor();
	Super::EndPlay(EndPlayReason);
}

void UPRWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPRWeaponComponent, EquippedWeapon);
	DOREPLIFETIME_CONDITION(UPRWeaponComponent, MagazineAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UPRWeaponComponent, bAiming);
	DOREPLIFETIME(UPRWeaponComponent, bReloading);
}

UPRWeaponDataAsset* UPRWeaponComponent::GetEquippedWeaponData() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	return Inventory && EquippedWeapon.IsValid()
		? Cast<UPRWeaponDataAsset>(Inventory->FindItemData(EquippedWeapon.ItemId))
		: nullptr;
}

int32 UPRWeaponComponent::GetReserveAmmo() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	return Inventory && WeaponData ? Inventory->GetItemCount(WeaponData->AmmoItemId) : 0;
}

bool UPRWeaponComponent::CanUseWeapon() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !CurrentAvatar || !GetEquippedWeaponData() || !ASC)
	{
		return false;
	}
	return !ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
		&& !ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Downed)
		&& !ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered)
		&& !ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned);
}

void UPRWeaponComponent::SetStateTag(const FGameplayTag Tag, const bool bEnabled)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!ASC || !Tag.IsValid())
	{
		return;
	}
	if (bEnabled)
	{
		ASC->AddLooseGameplayTag(Tag);
	}
	else
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}
}

bool UPRWeaponComponent::SetAimingAuthoritative(const bool bNewAiming)
{
	if (bNewAiming && (bReloading || !CanUseWeapon()))
	{
		return false;
	}
	if (bAiming == bNewAiming)
	{
		return true;
	}
	bAiming = bNewAiming;
	SetStateTag(ProjectRiftGameplayTags::State_Aiming, bAiming);
	if (CurrentAvatar)
	{
		CurrentAvatar->ApplyWeaponAimPresentation(bAiming);
	}
	NotifyStateChanged();
	return true;
}

void UPRWeaponComponent::SetAiming(const bool bNewAiming)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SetAimingAuthoritative(bNewAiming);
		return;
	}
	if (bNewAiming)
	{
		const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
		const UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
		if (bReloading || !CurrentAvatar || !GetEquippedWeaponData() || !ASC
			|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
			|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Downed)
			|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered)
			|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned))
		{
			return;
		}
	}
	if (CurrentAvatar && CurrentAvatar->IsLocallyControlled())
	{
		bAiming = bNewAiming;
		CurrentAvatar->ApplyWeaponAimPresentation(bNewAiming);
		NotifyStateChanged();
	}
	ServerSetAiming(bNewAiming);
}

void UPRWeaponComponent::ServerSetAiming_Implementation(const bool bNewAiming)
{
	const bool bAccepted = SetAimingAuthoritative(bNewAiming);
	ClientAimingResult(bAccepted, bAiming);
}

void UPRWeaponComponent::ClientAimingResult_Implementation(const bool bAccepted, const bool bAuthoritativeAiming)
{
	if (!bAccepted || bAiming != bAuthoritativeAiming)
	{
		bAiming = bAuthoritativeAiming;
		if (CurrentAvatar)
		{
			CurrentAvatar->ApplyWeaponAimPresentation(bAiming);
		}
		NotifyStateChanged();
	}
}

bool UPRWeaponComponent::StartReload()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return BeginReloadAuthoritative();
	}
	ServerStartReload();
	return true;
}

void UPRWeaponComponent::ServerStartReload_Implementation()
{
	BeginReloadAuthoritative();
}

bool UPRWeaponComponent::BeginReloadAuthoritative()
{
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	UWorld* World = GetWorld();
	if (!World || !WeaponData || !FMath::IsFinite(WeaponData->ReloadDuration) || WeaponData->ReloadDuration < 0.0f
		|| bReloading || !CanUseWeapon()
		|| MagazineAmmo >= WeaponData->MagazineCapacity || GetReserveAmmo() <= 0)
	{
		return false;
	}
	SetAimingAuthoritative(false);
	SetReloadingAuthoritative(true);
	if (WeaponData->ReloadDuration <= 0.0f)
	{
		FinishReload();
	}
	else
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UPRWeaponComponent::FinishReload, WeaponData->ReloadDuration, false);
	}
	return true;
}

void UPRWeaponComponent::FinishReload()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (Inventory && WeaponData && CanUseWeapon())
	{
		const int32 Count = FMath::Min(
			FMath::Max(0, WeaponData->MagazineCapacity - MagazineAmmo),
			Inventory->GetItemCount(WeaponData->AmmoItemId));
		if (Count > 0 && Inventory->RemoveItem(WeaponData->AmmoItemId, Count))
		{
			MagazineAmmo += Count;
		}
	}
	SetReloadingAuthoritative(false);
}

void UPRWeaponComponent::CancelReload()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CancelReloadAuthoritative();
	}
	else
	{
		ServerCancelReload();
	}
}

void UPRWeaponComponent::ServerCancelReload_Implementation()
{
	CancelReloadAuthoritative();
}

void UPRWeaponComponent::CancelReloadAuthoritative()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}
	SetReloadingAuthoritative(false);
}

void UPRWeaponComponent::SetReloadingAuthoritative(const bool bNewReloading)
{
	if (bReloading == bNewReloading)
	{
		return;
	}
	bReloading = bNewReloading;
	SetStateTag(ProjectRiftGameplayTags::State_Reloading, bReloading);
	NotifyStateChanged();
}

EPRWeaponFireResult UPRWeaponComponent::TryFire()
{
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (!WeaponData)
	{
		return EPRWeaponFireResult::NoWeapon;
	}
	if (!IsValidFireConfiguration(WeaponData))
	{
		return EPRWeaponFireResult::Invalid;
	}
	if (bReloading)
	{
		return EPRWeaponFireResult::Reloading;
	}
	if (!CanUseWeapon())
	{
		return EPRWeaponFireResult::Inactive;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return EPRWeaponFireResult::Invalid;
	}
	const double Now = World->GetTimeSeconds();
	if ((Now - LastFireTime) + UE_DOUBLE_SMALL_NUMBER < WeaponData->MinFireInterval)
	{
		return EPRWeaponFireResult::RateLimited;
	}
	if (MagazineAmmo <= 0)
	{
		return EPRWeaponFireResult::Empty;
	}

	const UCameraComponent* Camera = CurrentAvatar ? CurrentAvatar->GetFollowCamera() : nullptr;
	if (!Camera)
	{
		return EPRWeaponFireResult::Invalid;
	}
	LastFireTime = Now;
	--MagazineAmmo;
	NotifyStateChanged();

	const float SpreadRadians = FMath::DegreesToRadians(bAiming ? WeaponData->AimSpreadDegrees : WeaponData->HipSpreadDegrees);
	const FVector ViewStart = Camera->GetComponentLocation();
	const FRotator AimRotation = CurrentAvatar && CurrentAvatar->GetController()
		? CurrentAvatar->GetController()->GetControlRotation()
		: Camera->GetComponentRotation();
	const FVector ViewDirection = FMath::VRandCone(AimRotation.Vector(), SpreadRadians).GetSafeNormal();
	const FVector ViewEnd = ViewStart + ViewDirection * WeaponData->Range;
	FCollisionQueryParams ViewParams(SCENE_QUERY_STAT(PRWeaponViewTrace), true, CurrentAvatar);
	ViewParams.AddIgnoredActor(CurrentAvatar);
	if (SpawnedWeaponActor)
	{
		ViewParams.AddIgnoredActor(SpawnedWeaponActor);
	}
	FHitResult ViewHit;
	const FCollisionObjectQueryParams WeaponTraceObjects = MakeWeaponTraceObjectQuery();
	const bool bViewBlocked = World->LineTraceSingleByObjectType(ViewHit, ViewStart, ViewEnd, WeaponTraceObjects, ViewParams);
	const FVector AimPoint = bViewBlocked ? ViewHit.ImpactPoint : ViewEnd;

	FVector MuzzleStart = CurrentAvatar->GetActorLocation() + CurrentAvatar->GetActorForwardVector() * 60.0f;
	if (SpawnedWeaponActor)
	{
		MuzzleStart = SpawnedWeaponActor->GetMuzzleTransform().GetLocation();
	}
	FCollisionQueryParams MuzzleParams(SCENE_QUERY_STAT(PRWeaponMuzzleTrace), true, CurrentAvatar);
	MuzzleParams.AddIgnoredActor(CurrentAvatar);
	if (SpawnedWeaponActor)
	{
		MuzzleParams.AddIgnoredActor(SpawnedWeaponActor);
	}
	FHitResult MuzzleHit;
	const FVector MuzzleDirection = (AimPoint - MuzzleStart).GetSafeNormal();
	const FVector MuzzleEnd = bViewBlocked ? AimPoint + MuzzleDirection * 5.0f : AimPoint;
	const bool bMuzzleHit = World->LineTraceSingleByObjectType(MuzzleHit, MuzzleStart, MuzzleEnd, WeaponTraceObjects, MuzzleParams);
	if (!bMuzzleHit || !MuzzleHit.GetActor())
	{
		return EPRWeaponFireResult::FiredMiss;
	}

	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UAbilitySystemComponent* SourceASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MuzzleHit.GetActor());
	FPRDamageRequest DamageRequest;
	DamageRequest.BaseDamage = WeaponData->BaseDamage;
	DamageRequest.DamageType = WeaponData->DamageType;
	DamageRequest.HitResult = MuzzleHit;
	DamageRequest.HitReaction = WeaponData->HitReaction;
	DamageRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetAndSource;
	return SourceASC && TargetASC && UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
		SourceASC,
		TargetASC,
		DamageRequest,
		this)
		? EPRWeaponFireResult::FiredHit
		: EPRWeaponFireResult::FiredMiss;
}

int32 UPRWeaponComponent::FindPrimaryEquipmentIndex() const
{
	return EquipmentEntries.IndexOfByPredicate([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == ProjectRiftWeaponSlots::Primary;
	});
}

void UPRWeaponComponent::SetPrimaryEquipment(const FPRItemInstance& Item)
{
	const int32 Index = FindPrimaryEquipmentIndex();
	if (Index == INDEX_NONE)
	{
		EquipmentEntries.Emplace(ProjectRiftWeaponSlots::Primary, Item);
	}
	else
	{
		EquipmentEntries[Index].Item = Item;
	}
}

void UPRWeaponComponent::ClearPrimaryEquipment()
{
	EquipmentEntries.RemoveAll([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == ProjectRiftWeaponSlots::Primary;
	});
}

bool UPRWeaponComponent::AddMagazineToInventory(const UPRWeaponDataAsset* WeaponData)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (MagazineAmmo <= 0)
	{
		return true;
	}
	FPRItemInstance Ammo;
	Ammo.ItemId = WeaponData ? WeaponData->AmmoItemId : NAME_None;
	Ammo.Count = MagazineAmmo;
	return Inventory && Ammo.IsValid() && Inventory->AddItem(Ammo);
}

bool UPRWeaponComponent::LoadMagazineFromInventory(const UPRWeaponDataAsset* WeaponData)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!Inventory || !WeaponData)
	{
		return false;
	}
	const int32 Count = FMath::Min(WeaponData->MagazineCapacity, Inventory->GetItemCount(WeaponData->AmmoItemId));
	MagazineAmmo = 0;
	if (Count > 0)
	{
		if (!Inventory->RemoveItem(WeaponData->AmmoItemId, Count))
		{
			return false;
		}
		MagazineAmmo = Count;
	}
	return true;
}

bool UPRWeaponComponent::RestoreTransaction(
	const TArray<FPRItemInstance>& InventoryItems,
	const TArray<FPRProfileEquipmentEntry>& Equipment,
	const FPRItemInstance& Weapon,
	const int32 Ammo)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!Inventory || !Inventory->ReplaceInventoryItems(InventoryItems))
	{
		return false;
	}
	EquipmentEntries = Equipment;
	EquippedWeapon = Weapon;
	MagazineAmmo = Ammo;
	RefreshWeaponActor();
	NotifyStateChanged();
	return true;
}

bool UPRWeaponComponent::EquipWeaponFromInventory(const FName ItemId)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UPRWeaponDataAsset* NewWeaponData = Inventory ? Cast<UPRWeaponDataAsset>(Inventory->FindItemData(ItemId)) : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !Inventory || Inventory->GetItemCount(ItemId) <= 0
		|| !NewWeaponData || !NewWeaponData->bCanEquip)
	{
		return false;
	}

	const TArray<FPRItemInstance> OldInventory = Inventory->GetInventoryItems();
	const TArray<FPRProfileEquipmentEntry> OldEquipment = EquipmentEntries;
	const FPRItemInstance OldWeapon = EquippedWeapon;
	const int32 OldMagazine = MagazineAmmo;
	const int32 PrimaryIndex = FindPrimaryEquipmentIndex();
	const FPRItemInstance OldPrimary = PrimaryIndex != INDEX_NONE ? EquipmentEntries[PrimaryIndex].Item : FPRItemInstance();
	const UPRWeaponDataAsset* OldWeaponData = GetEquippedWeaponData();
	CancelReloadAuthoritative();
	SetAimingAuthoritative(false);

	if (!Inventory->RemoveItem(ItemId, 1)
		|| (OldWeaponData && !AddMagazineToInventory(OldWeaponData))
		|| (OldPrimary.IsValid() && !Inventory->AddItem(OldPrimary)))
	{
		RestoreTransaction(OldInventory, OldEquipment, OldWeapon, OldMagazine);
		return false;
	}

	FPRItemInstance NewWeapon;
	NewWeapon.ItemId = ItemId;
	NewWeapon.Count = 1;
	EquippedWeapon = NewWeapon;
	SetPrimaryEquipment(NewWeapon);
	MagazineAmmo = 0;
	if (!LoadMagazineFromInventory(NewWeaponData))
	{
		RestoreTransaction(OldInventory, OldEquipment, OldWeapon, OldMagazine);
		return false;
	}
	RefreshWeaponActor();
	NotifyStateChanged();
	return true;
}

bool UPRWeaponComponent::UnequipWeapon()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const int32 PrimaryIndex = FindPrimaryEquipmentIndex();
	if (!PlayerState || !PlayerState->HasAuthority() || !Inventory || PrimaryIndex == INDEX_NONE)
	{
		return false;
	}

	const TArray<FPRItemInstance> OldInventory = Inventory->GetInventoryItems();
	const TArray<FPRProfileEquipmentEntry> OldEquipment = EquipmentEntries;
	const FPRItemInstance OldWeapon = EquippedWeapon;
	const int32 OldMagazine = MagazineAmmo;
	const FPRItemInstance OldPrimary = EquipmentEntries[PrimaryIndex].Item;
	const UPRWeaponDataAsset* OldWeaponData = GetEquippedWeaponData();
	CancelReloadAuthoritative();
	SetAimingAuthoritative(false);
	if ((OldWeaponData && !AddMagazineToInventory(OldWeaponData)) || !Inventory->AddItem(OldPrimary))
	{
		RestoreTransaction(OldInventory, OldEquipment, OldWeapon, OldMagazine);
		return false;
	}
	ClearPrimaryEquipment();
	EquippedWeapon = FPRItemInstance();
	MagazineAmmo = 0;
	DestroyWeaponActor();
	NotifyStateChanged();
	return true;
}

bool UPRWeaponComponent::ReplaceEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		OutDiagnostic = TEXT("Weapon equipment can only be applied on the authoritative PlayerState.");
		return false;
	}
	CancelReloadAuthoritative();
	SetAimingAuthoritative(false);
	EquipmentEntries = InEntries;
	EquippedWeapon = FPRItemInstance();
	MagazineAmmo = 0;
	const int32 PrimaryIndex = FindPrimaryEquipmentIndex();
	if (PrimaryIndex != INDEX_NONE)
	{
		const FPRItemInstance Candidate = EquipmentEntries[PrimaryIndex].Item;
		APRPlayerState* TypedPlayerState = Cast<APRPlayerState>(GetOwner());
		UPRInventoryComponent* Inventory = TypedPlayerState ? TypedPlayerState->GetInventoryComponent() : nullptr;
		UPRWeaponDataAsset* WeaponData = Inventory ? Cast<UPRWeaponDataAsset>(Inventory->FindItemData(Candidate.ItemId)) : nullptr;
		if (WeaponData && Candidate.IsValid())
		{
			EquippedWeapon = Candidate;
			EquippedWeapon.Count = 1;
			if (!LoadMagazineFromInventory(WeaponData))
			{
				OutDiagnostic = TEXT("Equipped weapon magazine could not be initialized from the ammo pool.");
				return false;
			}
		}
	}
	RefreshWeaponActor();
	NotifyStateChanged();
	return true;
}

bool UPRWeaponComponent::BuildPersistentBackpack(TArray<FPRItemInstance>& OutBackpack, FString& OutDiagnostic) const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (!Inventory)
	{
		OutDiagnostic = TEXT("Inventory is unavailable while building persistent weapon ammo.");
		return false;
	}
	OutBackpack = Inventory->GetInventoryItems();
	if (WeaponData && MagazineAmmo > 0)
	{
		FPRItemInstance Ammo;
		Ammo.ItemId = WeaponData->AmmoItemId;
		Ammo.Count = MagazineAmmo;
		if (!AddItemToSnapshot(OutBackpack, Ammo, Inventory->GetMaxStackCount(Ammo.ItemId)))
		{
			OutDiagnostic = TEXT("Loaded magazine could not be merged into the persistent ammo pool.");
			return false;
		}
	}
	return true;
}

bool UPRWeaponComponent::EnsureStarterWeapon(const FName StarterWeaponItemId, FString& OutDiagnostic)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UPRWeaponDataAsset* WeaponData = Inventory ? Cast<UPRWeaponDataAsset>(Inventory->FindItemData(StarterWeaponItemId)) : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !Inventory || !WeaponData)
	{
		OutDiagnostic = TEXT("Starter weapon data is unavailable.");
		return false;
	}
	if (Inventory->GetItemCount(StarterWeaponItemId) > 0
		|| EquipmentEntries.ContainsByPredicate([StarterWeaponItemId](const FPRProfileEquipmentEntry& Entry)
		{
			return Entry.Item.ItemId == StarterWeaponItemId;
		}))
	{
		return true;
	}

	const TArray<FPRItemInstance> OldInventory = Inventory->GetInventoryItems();
	const TArray<FPRProfileEquipmentEntry> OldEquipment = EquipmentEntries;
	const FPRItemInstance OldWeapon = EquippedWeapon;
	const int32 OldMagazine = MagazineAmmo;
	FPRItemInstance Ammo;
	Ammo.ItemId = WeaponData->AmmoItemId;
	Ammo.Count = WeaponData->MagazineCapacity + WeaponData->InitialReserveAmmo;
	FPRItemInstance Weapon;
	Weapon.ItemId = StarterWeaponItemId;
	Weapon.Count = 1;
	if (!Inventory->AddItem(Ammo))
	{
		OutDiagnostic = TEXT("Starter ammo does not fit in the inventory.");
		return false;
	}
	if (FindPrimaryEquipmentIndex() == INDEX_NONE)
	{
		EquippedWeapon = Weapon;
		SetPrimaryEquipment(Weapon);
		if (!LoadMagazineFromInventory(WeaponData))
		{
			RestoreTransaction(OldInventory, OldEquipment, OldWeapon, OldMagazine);
			OutDiagnostic = TEXT("Starter magazine initialization failed.");
			return false;
		}
	}
	else if (!Inventory->AddItem(Weapon))
	{
		RestoreTransaction(OldInventory, OldEquipment, OldWeapon, OldMagazine);
		OutDiagnostic = TEXT("Starter weapon does not fit beside preserved legacy equipment.");
		return false;
	}
	RefreshWeaponActor();
	NotifyStateChanged();
	return true;
}

void UPRWeaponComponent::CopyRuntimeStateFrom(const UPRWeaponComponent* SourceComponent)
{
	if (!SourceComponent || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	EquipmentEntries = SourceComponent->EquipmentEntries;
	EquippedWeapon = SourceComponent->EquippedWeapon;
	MagazineAmmo = SourceComponent->MagazineAmmo;
	bAiming = false;
	bReloading = false;
	LastFireTime = SourceComponent->LastFireTime;
	RefreshWeaponActor();
	NotifyStateChanged();
}

void UPRWeaponComponent::HandleAvatarChanged(APRCharacter* NewAvatar)
{
	if (CurrentAvatar == NewAvatar)
	{
		return;
	}
	if (!NewAvatar && GetOwner() && GetOwner()->HasAuthority())
	{
		CancelReloadAuthoritative();
		SetAimingAuthoritative(false);
	}
	CurrentAvatar = NewAvatar;
	RefreshWeaponActor();
	if (CurrentAvatar)
	{
		CurrentAvatar->ApplyWeaponAimPresentation(bAiming);
	}
}

void UPRWeaponComponent::RefreshWeaponActor()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	DestroyWeaponActor();
	const UPRWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (!CurrentAvatar || !WeaponData || !WeaponData->WeaponActorClass)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = CurrentAvatar;
	Params.Instigator = CurrentAvatar;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedWeaponActor = GetWorld()->SpawnActor<APRWeaponActor>(WeaponData->WeaponActorClass, Params);
	if (SpawnedWeaponActor)
	{
		SpawnedWeaponActor->AttachToComponent(CurrentAvatar->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponData->AttachSocketName);
		SpawnedWeaponActor->SetActorRelativeTransform(WeaponData->AttachTransform);
	}
}

void UPRWeaponComponent::DestroyWeaponActor()
{
	if (SpawnedWeaponActor && GetOwner() && GetOwner()->HasAuthority())
	{
		SpawnedWeaponActor->Destroy();
	}
	SpawnedWeaponActor = nullptr;
}

void UPRWeaponComponent::NotifyStateChanged()
{
	OnWeaponStateChanged.Broadcast();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UPRWeaponComponent::OnRep_EquippedWeapon()
{
	NotifyStateChanged();
}

void UPRWeaponComponent::OnRep_MagazineAmmo()
{
	NotifyStateChanged();
}

void UPRWeaponComponent::OnRep_Aiming()
{
	if (CurrentAvatar)
	{
		CurrentAvatar->ApplyWeaponAimPresentation(bAiming);
	}
	NotifyStateChanged();
}

void UPRWeaponComponent::OnRep_Reloading()
{
	NotifyStateChanged();
}
