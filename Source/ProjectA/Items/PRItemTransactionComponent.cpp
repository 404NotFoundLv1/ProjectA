#include "Items/PRItemTransactionComponent.h"

#include "Items/PRInventoryComponent.h"
#include "Items/PREquipmentComponent.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PRPickupActor.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"

UPRItemTransactionComponent::UPRItemTransactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPRItemTransactionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPRItemTransactionComponent, Revision, COND_OwnerOnly);
}

void UPRItemTransactionComponent::ServerSubmitItemTransaction_Implementation(const FPRItemTransactionRequest& Request)
{
	const FPRItemTransactionResult Result = ExecuteAuthoritativeTransaction(Request);
	ClientReceiveItemTransactionResult(Result);
}

void UPRItemTransactionComponent::ClientReceiveItemTransactionResult_Implementation(const FPRItemTransactionResult& Result)
{
	OnTransactionResult.Broadcast(Result);
}

void UPRItemTransactionComponent::CopyRuntimeStateFrom(const UPRItemTransactionComponent* SourceComponent)
{
	if (!SourceComponent)
	{
		return;
	}
	Revision = SourceComponent->Revision;
	RecentResults = SourceComponent->RecentResults;
	PendingResults = SourceComponent->PendingResults;
	PendingRequests = SourceComponent->PendingRequests;
}

void UPRItemTransactionComponent::ResetForNewProfileBinding()
{
	Revision = 0;
	RecentResults.Reset();
	PendingResults.Reset();
	PendingRequests.Reset();
}

FPRItemTransactionResult UPRItemTransactionComponent::ExecuteAuthoritativeTransaction(const FPRItemTransactionRequest& Request)
{
	return ResolveRequest(Request);
}

FPRItemTransactionResult UPRItemTransactionComponent::CompletePendingReload(
	const FGuid& TransactionId,
	const FName AmmoItemId,
	const int32 RequestedCount)
{
	FPRItemTransactionResult Result;
	const FPRItemTransactionResult* Pending = FindPendingResult(TransactionId);
	if (!Pending)
	{
		Result.TransactionId = TransactionId;
		Result.Intent = EPRItemTransactionIntent::BeginReload;
		Result.Status = EPRItemTransactionStatus::NotFound;
		Result.PreviousRevision = Revision;
		Result.CurrentRevision = Revision;
		Result.Diagnostic = TEXT("The reload transaction is not pending.");
		return Result;
	}

	Result = *Pending;
	Result.bWasReplay = false;
	Result.PreviousRevision = Revision;
	Result.CurrentRevision = Revision;
	RemovePendingTransaction(TransactionId);

	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (AmmoItemId.IsNone() || RequestedCount <= 0 || !Inventory)
	{
		Result.Status = EPRItemTransactionStatus::Cancelled;
		Result.Diagnostic = TEXT("Reload completion no longer has valid reserve ammunition or an inventory.");
		CacheFinalResult(Result);
		return Result;
	}

	TArray<FPRItemInstance> CandidateItems = Inventory->GetInventoryItems();
	int32 Remaining = RequestedCount;
	for (int32 Index = CandidateItems.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FPRItemInstance& Item = CandidateItems[Index];
		if (Item.ItemId != AmmoItemId || Item.Count <= 0)
		{
			continue;
		}
		const int32 Applied = FMath::Min(Item.Count, Remaining);
		Item.Count -= Applied;
		Remaining -= Applied;
		Result.AppliedCount += Applied;
		if (Item.Count == 0)
		{
			Result.RemovedInstanceGuids.Add(Item.InstanceGuid);
			CandidateItems.RemoveAt(Index);
		}
		else
		{
			Result.ChangedInstanceGuids.Add(Item.InstanceGuid);
		}
	}

	if (Result.AppliedCount <= 0)
	{
		Result.Status = EPRItemTransactionStatus::Cancelled;
		Result.Diagnostic = TEXT("Reload completion found no reserve ammunition at commit time.");
		CacheFinalResult(Result);
		return Result;
	}
	if (!Inventory->ReplaceInventoryItems(CandidateItems))
	{
		Result.Status = EPRItemTransactionStatus::InternalFailure;
		Result.AppliedCount = 0;
		Result.ChangedInstanceGuids.Reset();
		Result.RemovedInstanceGuids.Reset();
		Result.Diagnostic = TEXT("Reload completion could not commit its validated inventory delta.");
		CacheFinalResult(Result);
		return Result;
	}

	Result.Status = EPRItemTransactionStatus::Success;
	Result.CurrentRevision = ++Revision;
	Result.Diagnostic = TEXT("Reload completion committed against current authoritative reserve ammunition.");
	CacheFinalResult(Result);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	return Result;
}

FPRItemTransactionResult UPRItemTransactionComponent::CancelPendingReload(const FGuid& TransactionId, const FString& Diagnostic)
{
	FPRItemTransactionResult Result;
	const FPRItemTransactionResult* Pending = FindPendingResult(TransactionId);
	if (!Pending)
	{
		Result.TransactionId = TransactionId;
		Result.Intent = EPRItemTransactionIntent::BeginReload;
		Result.Status = EPRItemTransactionStatus::NotFound;
		Result.PreviousRevision = Revision;
		Result.CurrentRevision = Revision;
		Result.Diagnostic = TEXT("The reload transaction is not pending.");
		return Result;
	}

	Result = *Pending;
	Result.Status = EPRItemTransactionStatus::Cancelled;
	Result.PreviousRevision = Revision;
	Result.CurrentRevision = Revision;
	Result.Diagnostic = Diagnostic.IsEmpty() ? TEXT("Reload transaction was cancelled.") : Diagnostic;
	RemovePendingTransaction(TransactionId);
	CacheFinalResult(Result);
	return Result;
}

FPRItemTransactionResult UPRItemTransactionComponent::CompletePendingUse(
	const FGuid& TransactionId,
	const FName GrantedItemId,
	const int32 GrantedItemCount)
{
	FPRItemTransactionResult Result;
	const FPRItemTransactionResult* Pending = FindPendingResult(TransactionId);
	const FPRItemTransactionRequest* PendingRequest = FindPendingRequest(TransactionId);
	if (!Pending || !PendingRequest || Pending->Intent != EPRItemTransactionIntent::BeginUse)
	{
		Result.TransactionId = TransactionId;
		Result.Intent = EPRItemTransactionIntent::BeginUse;
		Result.Status = EPRItemTransactionStatus::NotFound;
		Result.PreviousRevision = Revision;
		Result.CurrentRevision = Revision;
		Result.Diagnostic = TEXT("The consumable use transaction is not pending.");
		return Result;
	}

	Result = *Pending;
	Result.bWasReplay = false;
	Result.PreviousRevision = Revision;
	Result.CurrentRevision = Revision;
	const FPRItemTransactionRequest Request = *PendingRequest;
	RemovePendingTransaction(TransactionId);

	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!Inventory || !Request.InstanceGuid.IsValid() || Request.Count != 1
		|| GrantedItemCount < 0 || (GrantedItemCount > 0 && GrantedItemId.IsNone()))
	{
		Result.Status = EPRItemTransactionStatus::Cancelled;
		Result.Diagnostic = TEXT("Consumable completion lost a valid inventory or grant contract.");
		CacheFinalResult(Result);
		return Result;
	}

	TArray<FPRItemInstance> CandidateItems = Inventory->GetInventoryItems();
	const int32 SourceIndex = CandidateItems.IndexOfByPredicate([&Request](const FPRItemInstance& Item)
	{
		return Item.InstanceGuid == Request.InstanceGuid && Item.Count > 0;
	});
	if (SourceIndex == INDEX_NONE)
	{
		Result.Status = EPRItemTransactionStatus::Cancelled;
		Result.Diagnostic = TEXT("Consumable completion could not find its original source instance.");
		CacheFinalResult(Result);
		return Result;
	}

	FPRItemInstance& SourceItem = CandidateItems[SourceIndex];
	--SourceItem.Count;
	if (SourceItem.Count == 0)
	{
		Result.RemovedInstanceGuids.Add(SourceItem.InstanceGuid);
		CandidateItems.RemoveAt(SourceIndex);
	}
	else
	{
		Result.ChangedInstanceGuids.Add(SourceItem.InstanceGuid);
	}

	if (GrantedItemCount > 0)
	{
		FPRItemInstance GrantItem;
		GrantItem.ItemId = GrantedItemId;
		GrantItem.Count = GrantedItemCount;
		const int32 MaxStackCount = Inventory->GetMaxStackCount(GrantedItemId);
		int32 Remaining = GrantedItemCount;
		for (FPRItemInstance& Candidate : CandidateItems)
		{
			if (Remaining <= 0 || !Candidate.HasEquivalentStackingState(GrantItem) || Candidate.Count >= MaxStackCount)
			{
				continue;
			}
			const int32 Applied = FMath::Min(Remaining, MaxStackCount - Candidate.Count);
			Candidate.Count += Applied;
			Remaining -= Applied;
			Result.ChangedInstanceGuids.AddUnique(Candidate.InstanceGuid);
		}
		while (Remaining > 0)
		{
			FPRItemInstance NewStack = GrantItem;
			NewStack.InstanceGuid = FGuid::NewGuid();
			NewStack.Count = FMath::Min(Remaining, MaxStackCount);
			CandidateItems.Add(NewStack);
			Result.CreatedInstanceGuids.Add(NewStack.InstanceGuid);
			Remaining -= NewStack.Count;
		}
	}

	if (!Inventory->ReplaceInventoryItems(CandidateItems))
	{
		Result.Status = EPRItemTransactionStatus::InternalFailure;
		Result.AppliedCount = 0;
		Result.ChangedInstanceGuids.Reset();
		Result.CreatedInstanceGuids.Reset();
		Result.RemovedInstanceGuids.Reset();
		Result.Diagnostic = TEXT("Consumable completion could not atomically commit its inventory delta.");
		CacheFinalResult(Result);
		return Result;
	}

	Result.Status = EPRItemTransactionStatus::Success;
	Result.AppliedCount = 1;
	Result.CurrentRevision = ++Revision;
	Result.Diagnostic = TEXT("Pending consumable use committed exactly once.");
	CacheFinalResult(Result);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	return Result;
}

FPRItemTransactionResult UPRItemTransactionComponent::CancelPendingUse(const FGuid& TransactionId, const FString& Diagnostic)
{
	FPRItemTransactionResult Result;
	const FPRItemTransactionResult* Pending = FindPendingResult(TransactionId);
	if (!Pending || Pending->Intent != EPRItemTransactionIntent::BeginUse)
	{
		Result.TransactionId = TransactionId;
		Result.Intent = EPRItemTransactionIntent::BeginUse;
		Result.Status = EPRItemTransactionStatus::NotFound;
		Result.PreviousRevision = Revision;
		Result.CurrentRevision = Revision;
		Result.Diagnostic = TEXT("The consumable use transaction is not pending.");
		return Result;
	}
	Result = *Pending;
	Result.Status = EPRItemTransactionStatus::Cancelled;
	Result.PreviousRevision = Revision;
	Result.CurrentRevision = Revision;
	Result.Diagnostic = Diagnostic.IsEmpty() ? TEXT("Consumable use was cancelled.") : Diagnostic;
	RemovePendingTransaction(TransactionId);
	CacheFinalResult(Result);
	return Result;
}

FPRItemTransactionResult UPRItemTransactionComponent::ResolveRequest(const FPRItemTransactionRequest& Request)
{
	FPRItemTransactionResult Result;
	Result.TransactionId = Request.Header.TransactionId;
	Result.Intent = Request.Intent;
	Result.PreviousRevision = Revision;
	Result.CurrentRevision = Revision;

	if (!Request.Header.IsValid() || Request.Intent == EPRItemTransactionIntent::None)
	{
		Result.Status = EPRItemTransactionStatus::InvalidRequest;
		Result.Diagnostic = TEXT("Transaction header or intent is invalid.");
		return Result;
	}
	if (const FPRItemTransactionResult* Cached = FindCachedResult(Request.Header.TransactionId))
	{
		Result = *Cached;
		Result.bWasReplay = true;
		return Result;
	}
	if (const FPRItemTransactionResult* Pending = FindPendingResult(Request.Header.TransactionId))
	{
		Result = *Pending;
		Result.bWasReplay = true;
		return Result;
	}
	if (Request.Header.ExpectedRevision != Revision)
	{
		Result.Status = EPRItemTransactionStatus::RevisionConflict;
		Result.Diagnostic = TEXT("Transaction revision does not match the authoritative revision.");
		CacheFinalResult(Result);
		return Result;
	}

	if (Request.Intent == EPRItemTransactionIntent::BeginReload || Request.Intent == EPRItemTransactionIntent::BeginUse)
	{
		if (Request.Intent == EPRItemTransactionIntent::BeginUse && (!Request.InstanceGuid.IsValid() || Request.Count != 1))
		{
			Result.Status = EPRItemTransactionStatus::InvalidRequest;
			Result.Diagnostic = TEXT("Pending consumable use requires one valid source instance.");
			CacheFinalResult(Result);
			return Result;
		}
		Result.Status = EPRItemTransactionStatus::Pending;
		Result.Diagnostic = Request.Intent == EPRItemTransactionIntent::BeginUse
			? TEXT("Consumable use has been accepted and is awaiting authoritative completion.")
			: TEXT("Reload has been accepted and is awaiting authoritative completion.");
		PendingResults.Add(Result);
		PendingRequests.Add(Request);
		return Result;
	}
	if (Request.Intent == EPRItemTransactionIntent::Pickup)
	{
		APRPickupActor* Pickup = Cast<APRPickupActor>(Request.TargetActor);
		if (!Pickup || !Pickup->CanBePickedUp())
		{
			Result.Status = EPRItemTransactionStatus::NotFound;
			Result.Diagnostic = TEXT("The requested world pickup is no longer available.");
			CacheFinalResult(Result);
			return Result;
		}

		const FPRItemInstance PickedItem = Pickup->GetItemInstance();
		if (!PickedItem.HasValidIdentity())
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The requested world pickup has no authoritative item identity.");
			CacheFinalResult(Result);
			return Result;
		}

		APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
		UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		if (!Inventory)
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The owning PlayerState inventory is unavailable.");
			CacheFinalResult(Result);
			return Result;
		}
		if (!Inventory->CanAddItem(PickedItem) || !Inventory->AddItem(PickedItem))
		{
			Result.Status = EPRItemTransactionStatus::CapacityExceeded;
			Result.Diagnostic = TEXT("The authoritative inventory cannot accept the world pickup.");
			CacheFinalResult(Result);
			return Result;
		}

		Pickup->SetPickedUp(true);
		Pickup->Destroy();
		Result.Status = EPRItemTransactionStatus::Success;
		Result.CurrentRevision = ++Revision;
		Result.ChangedInstanceGuids.Add(PickedItem.InstanceGuid);
		Result.Diagnostic = TEXT("Authoritative pickup transaction committed.");
		CacheFinalResult(Result);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
		return Result;
	}
	if (Request.Intent == EPRItemTransactionIntent::EquipPrimary || Request.Intent == EPRItemTransactionIntent::UnequipPrimary)
	{
		APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
		UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
		if (!Inventory || !Weapon)
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The authoritative inventory or primary weapon slot is unavailable.");
			CacheFinalResult(Result);
			return Result;
		}

		bool bCommitted = false;
		if (Request.Intent == EPRItemTransactionIntent::EquipPrimary)
		{
			if (!Request.InstanceGuid.IsValid())
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("Equipping requires a valid inventory item instance identity.");
				CacheFinalResult(Result);
				return Result;
			}
			const FPRItemInstance* SourceItem = Inventory->GetItems().FindByPredicate([&Request](const FPRItemInstance& Item)
			{
				return Item.InstanceGuid == Request.InstanceGuid;
			});
			if (!SourceItem)
			{
				Result.Status = EPRItemTransactionStatus::NotFound;
				Result.Diagnostic = TEXT("The requested weapon instance is not in the authoritative inventory.");
				CacheFinalResult(Result);
				return Result;
			}
			const FPRItemInstance SourceItemCopy = *SourceItem;
			bCommitted = Weapon->EquipWeaponFromInventory(SourceItemCopy.ItemId);
			if (bCommitted)
			{
				if (SourceItemCopy.Count == 1)
				{
					Result.RemovedInstanceGuids.Add(Request.InstanceGuid);
				}
				else
				{
					Result.ChangedInstanceGuids.Add(Request.InstanceGuid);
					Result.CreatedInstanceGuids.Add(Weapon->GetEquippedWeapon().InstanceGuid);
				}
			}
		}
		else
		{
			const FPRItemInstance Equipped = Weapon->GetEquippedWeapon();
			bCommitted = Weapon->UnequipWeapon();
			if (bCommitted && Equipped.HasValidIdentity())
			{
				Result.CreatedInstanceGuids.Add(Equipped.InstanceGuid);
			}
		}

		if (!bCommitted)
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The validated primary weapon equipment transaction could not be committed.");
			CacheFinalResult(Result);
			return Result;
		}
		Result.Status = EPRItemTransactionStatus::Success;
		Result.CurrentRevision = ++Revision;
		Result.Diagnostic = TEXT("Authoritative primary weapon equipment transaction committed.");
		CacheFinalResult(Result);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
		return Result;
	}
	if (Request.Intent == EPRItemTransactionIntent::Equip || Request.Intent == EPRItemTransactionIntent::Unequip)
	{
		APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
		UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		UPREquipmentComponent* Equipment = PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
		UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
		if (!Inventory || !Equipment || Request.SlotId.IsNone())
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The authoritative inventory or equipment slots are unavailable.");
			CacheFinalResult(Result);
			return Result;
		}
		if (Request.SlotId == ProjectRiftEquipmentSlots::Weapon)
		{
			FPRItemTransactionRequest LegacyRequest = Request;
			LegacyRequest.Intent = Request.Intent == EPRItemTransactionIntent::Equip
				? EPRItemTransactionIntent::EquipPrimary : EPRItemTransactionIntent::UnequipPrimary;
			return ResolveRequest(LegacyRequest);
		}
		if (Request.SlotId != ProjectRiftEquipmentSlots::Armor && Request.SlotId != ProjectRiftEquipmentSlots::Chip
			&& Request.SlotId != ProjectRiftEquipmentSlots::Tool)
		{
			Result.Status = EPRItemTransactionStatus::InvalidRequest;
			Result.Diagnostic = TEXT("The requested equipment slot is not supported.");
			CacheFinalResult(Result);
			return Result;
		}

		const TArray<FPRItemInstance> PreviousInventory = Inventory->GetInventoryItems();
		const TArray<FPRProfileEquipmentEntry> PreviousEquipment = Equipment->GetEquipmentEntries();
		TArray<FPRItemInstance> CandidateInventory = PreviousInventory;
		TArray<FPRProfileEquipmentEntry> CandidateEquipment = PreviousEquipment;
		FPRItemInstance ChangedItem;
		if (Request.Intent == EPRItemTransactionIntent::Equip)
		{
			if (!Request.InstanceGuid.IsValid())
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("Equipping requires an item instance identity.");
				CacheFinalResult(Result);
				return Result;
			}
			const int32 SourceIndex = CandidateInventory.IndexOfByPredicate([&Request](const FPRItemInstance& Item) { return Item.InstanceGuid == Request.InstanceGuid; });
			if (SourceIndex == INDEX_NONE)
			{
				Result.Status = EPRItemTransactionStatus::NotFound;
				Result.Diagnostic = TEXT("The requested equipment instance is not in the authoritative inventory.");
				CacheFinalResult(Result);
				return Result;
			}
			ChangedItem = CandidateInventory[SourceIndex];
			UPREquipmentDataAsset* Definition = Cast<UPREquipmentDataAsset>(Inventory->FindItemData(ChangedItem.ItemId));
			if (!Definition || !Definition->bCanEquip || UPREquipmentComponent::GetSlotId(Definition->EquipmentSlot) != Request.SlotId)
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("The item definition does not match the requested equipment slot.");
				CacheFinalResult(Result);
				return Result;
			}
			if (ChangedItem.Count != 1)
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("Equipment instances must occupy a single-item inventory stack.");
				CacheFinalResult(Result);
				return Result;
			}
			CandidateInventory.RemoveAt(SourceIndex);
			const int32 ExistingSlotIndex = CandidateEquipment.IndexOfByPredicate([&Request](const FPRProfileEquipmentEntry& Entry) { return Entry.SlotId == Request.SlotId; });
			if (ExistingSlotIndex != INDEX_NONE)
			{
				if (!Inventory->CanAddItem(CandidateEquipment[ExistingSlotIndex].Item))
				{
					Result.Status = EPRItemTransactionStatus::CapacityExceeded;
					Result.Diagnostic = TEXT("The replaced equipment cannot return to the inventory.");
					CacheFinalResult(Result);
					return Result;
				}
				CandidateInventory.Add(CandidateEquipment[ExistingSlotIndex].Item);
				CandidateEquipment.RemoveAt(ExistingSlotIndex);
			}
			CandidateEquipment.Emplace(Request.SlotId, ChangedItem);
		}
		else
		{
			const int32 EquippedIndex = CandidateEquipment.IndexOfByPredicate([&Request](const FPRProfileEquipmentEntry& Entry) { return Entry.SlotId == Request.SlotId; });
			if (EquippedIndex == INDEX_NONE)
			{
				Result.Status = EPRItemTransactionStatus::NotFound;
				Result.Diagnostic = TEXT("No item is equipped in the requested slot.");
				CacheFinalResult(Result);
				return Result;
			}
			ChangedItem = CandidateEquipment[EquippedIndex].Item;
			if (!Inventory->CanAddItem(ChangedItem))
			{
				Result.Status = EPRItemTransactionStatus::CapacityExceeded;
				Result.Diagnostic = TEXT("The equipped item cannot return to the inventory.");
				CacheFinalResult(Result);
				return Result;
			}
			CandidateEquipment.RemoveAt(EquippedIndex);
			CandidateInventory.Add(ChangedItem);
		}

		FString Diagnostic;
		if (!Inventory->ReplaceInventoryItems(CandidateInventory) || !Equipment->ReplaceEquipmentEntries(CandidateEquipment, Diagnostic))
		{
			Inventory->ReplaceInventoryItems(PreviousInventory);
			Equipment->ReplaceEquipmentEntries(PreviousEquipment, Diagnostic);
			Result.Status = EPRItemTransactionStatus::InternalFailure;
			Result.Diagnostic = TEXT("The equipment transaction did not commit and was restored.");
			CacheFinalResult(Result);
			return Result;
		}
		Result.Status = EPRItemTransactionStatus::Success;
		Result.CurrentRevision = ++Revision;
		if (Request.Intent == EPRItemTransactionIntent::Equip)
		{
			Result.RemovedInstanceGuids.Add(ChangedItem.InstanceGuid);
		}
		else
		{
			Result.CreatedInstanceGuids.Add(ChangedItem.InstanceGuid);
		}
		Result.Diagnostic = TEXT("Authoritative equipment transaction committed.");
		CacheFinalResult(Result);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
		return Result;
	}
	if (Request.Intent == EPRItemTransactionIntent::Use || Request.Intent == EPRItemTransactionIntent::Drop)
	{
		if (!Request.InstanceGuid.IsValid() || Request.Count <= 0 || (Request.Intent == EPRItemTransactionIntent::Use && Request.Count != 1))
		{
			Result.Status = EPRItemTransactionStatus::InvalidRequest;
			Result.Diagnostic = TEXT("The item instance identity or requested quantity is invalid.");
			CacheFinalResult(Result);
			return Result;
		}
		APRPickupActor* DropDestination = nullptr;
		if (Request.Intent == EPRItemTransactionIntent::Drop)
		{
			DropDestination = Cast<APRPickupActor>(Request.TargetActor);
			if (!DropDestination || !DropDestination->CanBePickedUp())
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("A drop transaction requires a prepared, available world pickup destination.");
				CacheFinalResult(Result);
				return Result;
			}
		}

		APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
		UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
		if (!Inventory)
		{
			Result.Status = EPRItemTransactionStatus::InvalidState;
			Result.Diagnostic = TEXT("The owning PlayerState inventory is unavailable.");
			CacheFinalResult(Result);
			return Result;
		}

		TArray<FPRItemInstance> CandidateItems = Inventory->GetInventoryItems();
		const int32 ItemIndex = CandidateItems.IndexOfByPredicate([&Request](const FPRItemInstance& Item)
		{
			return Item.InstanceGuid == Request.InstanceGuid;
		});
		if (ItemIndex == INDEX_NONE)
		{
			Result.Status = EPRItemTransactionStatus::NotFound;
			Result.Diagnostic = TEXT("The requested item instance is not in the authoritative inventory.");
			CacheFinalResult(Result);
			return Result;
		}
		if (CandidateItems[ItemIndex].Count < Request.Count)
		{
			Result.Status = EPRItemTransactionStatus::InsufficientQuantity;
			Result.Diagnostic = TEXT("The requested item quantity exceeds the authoritative stack count.");
			CacheFinalResult(Result);
			return Result;
		}
		const FPRItemInstance SourceItem = CandidateItems[ItemIndex];
		if (DropDestination)
		{
			const FPRItemInstance DestinationItem = DropDestination->GetItemInstance();
			const bool bMatchesSourceContents = DestinationItem.IsValid()
				&& DestinationItem.HasEquivalentStackingState(SourceItem)
				&& DestinationItem.Count == Request.Count;
			const bool bTransfersWholeInstance = SourceItem.Count == Request.Count;
			const bool bIdentityMatchesTransfer = DestinationItem.HasValidIdentity()
				&& (bTransfersWholeInstance
					? DestinationItem.InstanceGuid == SourceItem.InstanceGuid
					: DestinationItem.InstanceGuid != SourceItem.InstanceGuid);
			if (!bMatchesSourceContents || !bIdentityMatchesTransfer)
			{
				Result.Status = EPRItemTransactionStatus::InvalidRequest;
				Result.Diagnostic = TEXT("The prepared world pickup does not match the authoritative source instance split contract.");
				CacheFinalResult(Result);
				return Result;
			}
		}

		CandidateItems[ItemIndex].Count -= Request.Count;
		if (CandidateItems[ItemIndex].Count == 0)
		{
			CandidateItems.RemoveAt(ItemIndex);
			Result.RemovedInstanceGuids.Add(Request.InstanceGuid);
		}
		else
		{
			Result.ChangedInstanceGuids.Add(Request.InstanceGuid);
			if (DropDestination)
			{
				Result.CreatedInstanceGuids.Add(DropDestination->GetItemInstance().InstanceGuid);
			}
		}
		if (!Inventory->ReplaceInventoryItems(CandidateItems))
		{
			Result.Status = EPRItemTransactionStatus::InternalFailure;
			Result.Diagnostic = TEXT("The validated inventory transaction could not be committed.");
			Result.ChangedInstanceGuids.Reset();
			Result.RemovedInstanceGuids.Reset();
			CacheFinalResult(Result);
			return Result;
		}

		Result.Status = EPRItemTransactionStatus::Success;
		Result.CurrentRevision = ++Revision;
		Result.Diagnostic = TEXT("Authoritative item transaction committed.");
		CacheFinalResult(Result);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
		return Result;
	}

	Result.Status = EPRItemTransactionStatus::InvalidState;
	Result.Diagnostic = TEXT("The requested item transaction is not implemented by the authoritative executor yet.");
	CacheFinalResult(Result);
	return Result;
}

void UPRItemTransactionComponent::CacheFinalResult(const FPRItemTransactionResult& Result)
{
	if (!Result.TransactionId.IsValid() || Result.Status == EPRItemTransactionStatus::Pending)
	{
		return;
	}
	RecentResults.Add(Result);
	if (RecentResults.Num() > MaxCachedResults)
	{
		RecentResults.RemoveAt(0, RecentResults.Num() - MaxCachedResults);
	}
}

const FPRItemTransactionResult* UPRItemTransactionComponent::FindCachedResult(const FGuid& TransactionId) const
{
	return RecentResults.FindByPredicate([TransactionId](const FPRItemTransactionResult& Result)
	{
		return Result.TransactionId == TransactionId;
	});
}

const FPRItemTransactionResult* UPRItemTransactionComponent::FindPendingResult(const FGuid& TransactionId) const
{
	return PendingResults.FindByPredicate([TransactionId](const FPRItemTransactionResult& Result)
	{
		return Result.TransactionId == TransactionId;
	});
}

const FPRItemTransactionRequest* UPRItemTransactionComponent::FindPendingRequest(const FGuid& TransactionId) const
{
	return PendingRequests.FindByPredicate([TransactionId](const FPRItemTransactionRequest& Request)
	{
		return Request.Header.TransactionId == TransactionId;
	});
}

void UPRItemTransactionComponent::RemovePendingTransaction(const FGuid& TransactionId)
{
	PendingResults.RemoveAll([TransactionId](const FPRItemTransactionResult& Candidate)
	{
		return Candidate.TransactionId == TransactionId;
	});
	PendingRequests.RemoveAll([TransactionId](const FPRItemTransactionRequest& Candidate)
	{
		return Candidate.Header.TransactionId == TransactionId;
	});
}
