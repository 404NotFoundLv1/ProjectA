#include "Persistence/PRProfileRuntimeBridge.h"

#include "Items/PRInventoryComponent.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"
#include "Weapons/PRWeaponTypes.h"

bool FPRProfileRuntimeBridge::CaptureFromPlayerState(
	const APRPlayerState* PlayerState,
	FPRProfileSnapshot& InOutSnapshot,
	FString& OutDiagnostic)
{
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	const UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
	if (!PlayerState || !Inventory || !Weapon)
	{
		OutDiagnostic = TEXT("PlayerState, inventory, or weapon component is unavailable.");
		return false;
	}
	if (!Weapon->BuildPersistentBackpack(InOutSnapshot.BackpackItems, OutDiagnostic))
	{
		return false;
	}

	InOutSnapshot.Equipment.RemoveAll([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == ProjectRiftWeaponSlots::Primary;
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
	InOutSnapshot.SelectedRoleId = PlayerState->GetSelectedRoleModule();
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
	if (!PlayerState || !Inventory || !Weapon)
	{
		OutDiagnostic = TEXT("PlayerState, inventory, or weapon component is unavailable.");
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
	const FName PreviousRole = PlayerState->GetSelectedRoleModule();

	if (!Inventory->ReplaceInventoryItems(Snapshot.BackpackItems)
		|| !PlayerState->ReplaceShipResources(Resources)
		|| !Weapon->ReplaceEquipmentEntries(Snapshot.Equipment, OutDiagnostic)
		|| !Weapon->EnsureStarterWeapon(TEXT("TestRifle"), OutDiagnostic))
	{
		Inventory->ReplaceInventoryItems(PreviousBackpack);
		PlayerState->ReplaceShipResources(PreviousResources);
		FString RestoreDiagnostic;
		Weapon->ReplaceEquipmentEntries(PreviousEquipment, RestoreDiagnostic);
		PlayerState->SetSelectedRoleModule(PreviousRole);
		if (OutDiagnostic.IsEmpty())
		{
			OutDiagnostic = TEXT("PlayerState rejected validated profile data.");
		}
		return false;
	}
	PlayerState->SetSelectedRoleModule(Snapshot.SelectedRoleId);
	return true;
}
