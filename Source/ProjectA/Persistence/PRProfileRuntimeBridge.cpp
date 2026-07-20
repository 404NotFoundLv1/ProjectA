#include "Persistence/PRProfileRuntimeBridge.h"

#include "Core/PRAssetManager.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRQuickbarComponent.h"
#include "Items/PREquipmentTypes.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleComponent.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Weapons/PRWeaponComponent.h"
#include "Weapons/PRWeaponTypes.h"

namespace
{
bool AreRoleLoadoutsEqual(const FPRRoleLoadout& Left, const FPRRoleLoadout& Right)
{
	if (Left.Entries.Num() != Right.Entries.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Entries.Num(); ++Index)
	{
		if (Left.Entries[Index].Slot != Right.Entries[Index].Slot
			|| Left.Entries[Index].ModuleId != Right.Entries[Index].ModuleId)
		{
			return false;
		}
	}
	return true;
}

bool ContainsAllRoleIds(const TArray<FName>& Superset, const TArray<FName>& Required)
{
	return Required.ContainsByPredicate([&Superset](const FName Id) { return !Superset.Contains(Id); }) == false;
}

bool IsStarterUnlockExpansion(
	const FName RequestedRoleId,
	const TArray<FName>& RequestedRoleIds,
	const TArray<FName>& RequestedModuleIds,
	const FPRRoleLoadout& RequestedLoadout,
	const FName AppliedRoleId,
	const TArray<FName>& AppliedRoleIds,
	const TArray<FName>& AppliedModuleIds,
	const FPRRoleLoadout& AppliedLoadout)
{
	return AppliedRoleId == RequestedRoleId
		&& AreRoleLoadoutsEqual(AppliedLoadout, RequestedLoadout)
		&& ContainsAllRoleIds(AppliedRoleIds, RequestedRoleIds)
		&& ContainsAllRoleIds(AppliedModuleIds, RequestedModuleIds);
}

bool IsKnownLegacyRole(const FName SelectedRoleId)
{
	const FName NormalizedRoleId = SelectedRoleId.IsNone() || SelectedRoleId == TEXT("Role.Assault")
		? FName(TEXT("Ability.Role.Assault"))
		: SelectedRoleId;
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> Modules;
	return AssetManager && AssetManager->LoadRoleCatalog(Roles, Modules)
		&& Roles.ContainsByPredicate([NormalizedRoleId](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId == NormalizedRoleId;
		});
}
}

bool FPRProfileRuntimeBridge::CaptureFromPlayerState(
	const APRPlayerState* PlayerState,
	FPRProfileSnapshot& InOutSnapshot,
	FString& OutDiagnostic)
{
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
	const UPRRoleComponent* RoleComponent = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	const UPRQuickbarComponent* Quickbar = PlayerState ? PlayerState->GetQuickbarComponent() : nullptr;
	if (!PlayerState || !Inventory || !Weapon || !RoleComponent || !Quickbar)
	{
		OutDiagnostic = TEXT("PlayerState, inventory, weapon, role, or quickbar component is unavailable.");
		return false;
	}
	if (!Weapon->BuildPersistentBackpack(InOutSnapshot.BackpackItems, OutDiagnostic))
	{
		return false;
	}
	InOutSnapshot.QuickSlots = Quickbar->GetQuickSlots();

	InOutSnapshot.Equipment.RemoveAll([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == ProjectRiftEquipmentSlots::Weapon
			|| Entry.SlotId == ProjectRiftEquipmentSlots::Armor
			|| Entry.SlotId == ProjectRiftEquipmentSlots::Chip
			|| Entry.SlotId == ProjectRiftEquipmentSlots::Tool;
	});
	for (const FPRProfileEquipmentEntry& RuntimeEntry : Weapon->GetEquipmentEntries())
	{
		const int32 ExistingIndex = InOutSnapshot.Equipment.IndexOfByPredicate([&RuntimeEntry](const FPRProfileEquipmentEntry& Entry)
		{
			return Entry.SlotId == RuntimeEntry.SlotId;
		});
		if (ExistingIndex == INDEX_NONE)
		{
			InOutSnapshot.Equipment.Add(RuntimeEntry);
		}
		else
		{
			InOutSnapshot.Equipment[ExistingIndex] = RuntimeEntry;
		}
	}
	InOutSnapshot.ResourceWallet.Reset();
	for (const FPRShipResourceStack& Resource : PlayerState->GetShipResources())
	{
		if (Resource.IsValid())
		{
			InOutSnapshot.ResourceWallet.Emplace(Resource.ResourceId, Resource.Count);
		}
	}
	FPRRoleLoadout RoleLoadout;
	RoleComponent->CaptureProfileRoleState(
		InOutSnapshot.SelectedRoleId,
		InOutSnapshot.UnlockedRoleIds,
		RoleLoadout,
		InOutSnapshot.UnlockedRoleModuleIds);
	InOutSnapshot.EquippedRoleModules = RoleLoadout.Entries;
	InOutSnapshot.Normalize();
	return InOutSnapshot.IsValid(&OutDiagnostic);
}

bool FPRProfileRuntimeBridge::ApplyToPlayerState(
	const FPRProfileSnapshot& Snapshot,
	APRPlayerState* PlayerState,
	FString& OutDiagnostic)
{
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
	UPRRoleComponent* RoleComponent = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	UPRQuickbarComponent* Quickbar = PlayerState ? PlayerState->GetQuickbarComponent() : nullptr;
	if (!PlayerState || !Inventory || !Weapon || !RoleComponent || !Quickbar)
	{
		OutDiagnostic = TEXT("PlayerState, inventory, weapon, role, or quickbar component is unavailable.");
		return false;
	}
	if (!Snapshot.IsValid(&OutDiagnostic))
	{
		return false;
	}
	TArray<FPRShipResourceStack> Resources;
	Resources.Reserve(Snapshot.ResourceWallet.Num());
	for (const FPRProfileResourceBalance& Balance : Snapshot.ResourceWallet)
	{
		FPRShipResourceStack& Resource = Resources.AddDefaulted_GetRef();
		Resource.ResourceId = Balance.ResourceId;
		Resource.Count = Balance.Count;
	}
	TArray<FPRItemInstance> PreviousBackpack;
	if (!Weapon->BuildPersistentBackpack(PreviousBackpack, OutDiagnostic))
	{
		return false;
	}
	const TArray<FPRProfileEquipmentEntry> PreviousEquipment = Weapon->GetEquipmentEntries();
	const TArray<FPRShipResourceStack> PreviousResources = PlayerState->GetShipResources();
	FName PreviousRoleId;
	TArray<FName> PreviousUnlockedRoleIds;
	TArray<FName> PreviousUnlockedModuleIds;
	FPRRoleLoadout PreviousLoadout;
	RoleComponent->CaptureProfileRoleState(
		PreviousRoleId,
		PreviousUnlockedRoleIds,
		PreviousLoadout,
		PreviousUnlockedModuleIds);
	FPRRoleLoadout RequestedLoadout;
	RequestedLoadout.Entries = Snapshot.EquippedRoleModules;
	const bool bLegacyRolePayload = Snapshot.EquippedRoleModules.IsEmpty()
		&& Snapshot.UnlockedRoleModuleIds.IsEmpty();
	const bool bLegacyKnownRole = bLegacyRolePayload && IsKnownLegacyRole(Snapshot.SelectedRoleId);
	auto RestorePreviousState = [&]()
	{
		Inventory->ReplaceInventoryItems(PreviousBackpack);
		PlayerState->ReplaceShipResources(PreviousResources);
		FString RestoreDiagnostic;
		Weapon->ReplaceEquipmentEntries(PreviousEquipment, RestoreDiagnostic);
		RoleComponent->ApplyProfileRoleState(
			PreviousRoleId,
			PreviousUnlockedRoleIds,
			PreviousLoadout,
			PreviousUnlockedModuleIds);
	};

	if (!Inventory->ReplaceInventoryItems(Snapshot.BackpackItems)
		|| !PlayerState->ReplaceShipResources(Resources)
		|| !Weapon->ReplaceEquipmentEntries(Snapshot.Equipment, OutDiagnostic)
		|| !Weapon->EnsureStarterWeapon(TEXT("TestRifle"), OutDiagnostic))
	{
		RestorePreviousState();
		if (OutDiagnostic.IsEmpty())
		{
			OutDiagnostic = TEXT("PlayerState rejected validated profile data.");
		}
		return false;
	}
	RoleComponent->ApplyProfileRoleState(
		Snapshot.SelectedRoleId,
		Snapshot.UnlockedRoleIds,
		RequestedLoadout,
		Snapshot.UnlockedRoleModuleIds);
	if (bLegacyKnownRole && !RoleComponent->EnsureDefaultLoadoutForSelectedRole())
	{
		RestorePreviousState();
		OutDiagnostic = TEXT("Legacy profile role defaults could not be applied.");
		return false;
	}
	FName AppliedRoleId;
	TArray<FName> AppliedUnlockedRoleIds;
	TArray<FName> AppliedUnlockedModuleIds;
	FPRRoleLoadout AppliedLoadout;
	RoleComponent->CaptureProfileRoleState(
		AppliedRoleId,
		AppliedUnlockedRoleIds,
		AppliedLoadout,
		AppliedUnlockedModuleIds);
	const bool bExactRolePayload = AppliedRoleId == Snapshot.SelectedRoleId
		&& AppliedUnlockedRoleIds == Snapshot.UnlockedRoleIds
		&& AppliedUnlockedModuleIds == Snapshot.UnlockedRoleModuleIds
		&& AreRoleLoadoutsEqual(AppliedLoadout, RequestedLoadout);
	const bool bStarterUnlocksExpanded = IsStarterUnlockExpansion(
		Snapshot.SelectedRoleId,
		Snapshot.UnlockedRoleIds,
		Snapshot.UnlockedRoleModuleIds,
		RequestedLoadout,
		AppliedRoleId,
		AppliedUnlockedRoleIds,
		AppliedUnlockedModuleIds,
		AppliedLoadout);
	if ((!bLegacyKnownRole && !bExactRolePayload && !bStarterUnlocksExpanded)
		|| (bLegacyKnownRole && !RoleComponent->IsLoadoutValid(AppliedRoleId, AppliedLoadout)))
	{
		RestorePreviousState();
		OutDiagnostic = TEXT("PlayerState rejected profile role state.");
		return false;
	}
	Quickbar->SetQuickSlotsFromProfile(Snapshot.QuickSlots);
	return true;
}
