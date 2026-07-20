#include "Items/PRInventoryComponent.h"

#include "Core/PRAssetManager.h"
#include "GameFramework/Actor.h"
#include "Items/PRItemDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"

void FPRInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (OwnerComponent)
	{
		OwnerComponent->HandleInventoryListChanged();
	}
}

void FPRInventoryList::SetOwnerComponent(UPRInventoryComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}

int32 FPRInventoryList::FindEntryIndex(const FName ItemId) const
{
	return Entries.IndexOfByPredicate(
		[ItemId](const FPRInventoryEntry& Entry)
		{
			return Entry.Item.ItemId == ItemId;
		});
}

UPRInventoryComponent::UPRInventoryComponent()
	: MaxSlots(20)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.SetOwnerComponent(this);
}

void UPRInventoryComponent::PostInitProperties()
{
	Super::PostInitProperties();

	InventoryList.SetOwnerComponent(this);
	RefreshCachedItems();
}

void UPRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPRInventoryComponent, InventoryList);
}

bool UPRInventoryComponent::CanAddItem(const FPRItemInstance& Item) const
{
	if (!Item.IsValid() || MaxSlots <= 0)
	{
		return false;
	}

	const int32 MaxStackCount = GetMaxStackCount(Item.ItemId);
	if (MaxStackCount <= 0)
	{
		return false;
	}

	int32 RemainingCount = Item.Count;
	for (const FPRInventoryEntry& Entry : InventoryList.Entries)
	{
		if (!CanStackItemInstances(Entry.Item, Item))
		{
			continue;
		}

		const int32 AvailableStackSpace = FMath::Max(0, MaxStackCount - Entry.Item.Count);
		RemainingCount -= FMath::Min(RemainingCount, AvailableStackSpace);
		if (RemainingCount <= 0)
		{
			return true;
		}
	}

	int32 RemainingFreeSlots = MaxSlots - InventoryList.Entries.Num();
	while (RemainingCount > 0 && RemainingFreeSlots > 0)
	{
		RemainingCount -= FMath::Min(RemainingCount, MaxStackCount);
		--RemainingFreeSlots;
	}

	return RemainingCount <= 0;
}

bool UPRInventoryComponent::AddItem(const FPRItemInstance& Item)
{
	if (!CanAddItem(Item))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory add rejected. Owner=%s ItemId=%s Count=%d Slots=%d/%d"),
			*GetNameSafe(GetOwner()),
			*Item.ItemId.ToString(),
			Item.Count,
			InventoryList.Entries.Num(),
			MaxSlots);
		return false;
	}
	if (Item.HasValidIdentity() && InventoryList.Entries.ContainsByPredicate([&Item](const FPRInventoryEntry& Entry)
	{
		return Entry.Item.InstanceGuid == Item.InstanceGuid;
	}))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Inventory add rejected duplicate item identity. Owner=%s InstanceGuid=%s"),
			*GetNameSafe(GetOwner()), *Item.InstanceGuid.ToString(EGuidFormats::DigitsWithHyphens));
		return false;
	}

	const int32 MaxStackCount = GetMaxStackCount(Item.ItemId);
	int32 RemainingCount = Item.Count;
	bool bCanPreserveIncomingIdentity = Item.HasValidIdentity();
	while (RemainingCount > 0)
	{
		const int32 ExistingIndex = FindStackableEntryIndex(Item);
		if (ExistingIndex != INDEX_NONE)
		{
			FPRInventoryEntry& ExistingEntry = InventoryList.Entries[ExistingIndex];
			const int32 AvailableStackSpace = FMath::Max(0, MaxStackCount - ExistingEntry.Item.Count);
			const int32 CountToAdd = FMath::Min(RemainingCount, AvailableStackSpace);
			ExistingEntry.Item.Count += CountToAdd;
			RemainingCount -= CountToAdd;
			InventoryList.MarkItemDirty(ExistingEntry);
			continue;
		}

		FPRInventoryEntry& NewEntry = InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Item = Item;
		if (!bCanPreserveIncomingIdentity)
		{
			NewEntry.Item.InstanceGuid = FGuid::NewGuid();
		}
		bCanPreserveIncomingIdentity = false;
		NewEntry.Item.Count = FMath::Min(RemainingCount, MaxStackCount);
		RemainingCount -= NewEntry.Item.Count;
		InventoryList.MarkItemDirty(NewEntry);
	}

	NotifyInventoryChanged();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Inventory added item. Owner=%s ItemId=%s Added=%d Total=%d Slots=%d/%d"),
		*GetNameSafe(GetOwner()),
		*Item.ItemId.ToString(),
		Item.Count,
		GetItemCount(Item.ItemId),
		InventoryList.Entries.Num(),
		MaxSlots);

	return true;
}

bool UPRInventoryComponent::RemoveItem(const FName ItemId, const int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	if (GetItemCount(ItemId) < Count)
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory remove rejected. Owner=%s ItemId=%s Requested=%d Available=%d"),
			*GetNameSafe(GetOwner()),
			*ItemId.ToString(),
			Count,
			GetItemCount(ItemId));
		return false;
	}

	int32 RemainingCount = Count;
	for (int32 EntryIndex = 0; EntryIndex < InventoryList.Entries.Num() && RemainingCount > 0;)
	{
		FPRInventoryEntry& ExistingEntry = InventoryList.Entries[EntryIndex];
		if (ExistingEntry.Item.ItemId != ItemId)
		{
			++EntryIndex;
			continue;
		}

		const int32 CountToRemove = FMath::Min(RemainingCount, ExistingEntry.Item.Count);
		ExistingEntry.Item.Count -= CountToRemove;
		RemainingCount -= CountToRemove;
		if (ExistingEntry.Item.Count <= 0)
		{
			InventoryList.Entries.RemoveAt(EntryIndex);
			InventoryList.MarkArrayDirty();
			continue;
		}

		InventoryList.MarkItemDirty(ExistingEntry);
		++EntryIndex;
	}

	NotifyInventoryChanged();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Inventory removed item. Owner=%s ItemId=%s Removed=%d Remaining=%d Slots=%d/%d"),
		*GetNameSafe(GetOwner()),
		*ItemId.ToString(),
		Count,
		GetItemCount(ItemId),
		InventoryList.Entries.Num(),
		MaxSlots);

	return true;
}

bool UPRInventoryComponent::UseItem(const FName ItemId)
{
	if (!RemoveItem(ItemId, 1))
	{
		return false;
	}

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Inventory used item. Owner=%s ItemId=%s"),
		*GetNameSafe(GetOwner()),
		*ItemId.ToString());

	return true;
}

bool UPRInventoryComponent::ReplaceInventoryItems(const TArray<FPRItemInstance>& Items)
{
	if (!CanReplaceInventoryItems(Items))
	{
		return false;
	}

	InventoryList.Entries.Reset(Items.Num());
	for (const FPRItemInstance& Item : Items)
	{
		FPRInventoryEntry& Entry = InventoryList.Entries.AddDefaulted_GetRef();
		Entry.Item = Item;
	}
	InventoryList.MarkArrayDirty();
	NotifyInventoryChanged();
	return true;
}

bool UPRInventoryComponent::CanReplaceInventoryItems(const TArray<FPRItemInstance>& Items) const
{
	if (Items.Num() > MaxSlots || Items.ContainsByPredicate([this](const FPRItemInstance& Item)
	{
		return !Item.IsValid() || Item.Count > GetMaxStackCount(Item.ItemId);
	}))
	{
		return false;
	}

	TSet<FGuid> SeenIdentities;
	for (const FPRItemInstance& Item : Items)
	{
		if (Item.HasValidIdentity() && SeenIdentities.Contains(Item.InstanceGuid))
		{
			return false;
		}
		if (Item.HasValidIdentity())
		{
			SeenIdentities.Add(Item.InstanceGuid);
		}
	}
	return true;
}

int32 UPRInventoryComponent::GetItemCount(const FName ItemId) const
{
	int32 TotalCount = 0;
	for (const FPRInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Item.ItemId == ItemId)
		{
			TotalCount += Entry.Item.Count;
		}
	}

	return TotalCount;
}

void UPRInventoryComponent::SetItemDataAssets(const TArray<UPRItemDataAsset*>& InItemDataAssets)
{
	ItemDataAssets.Reset(InItemDataAssets.Num());
	for (UPRItemDataAsset* ItemDataAsset : InItemDataAssets)
	{
		if (ItemDataAsset)
		{
			ItemDataAssets.Add(ItemDataAsset);
		}
	}
}

UPRItemDataAsset* UPRInventoryComponent::FindItemData(const FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}

	for (UPRItemDataAsset* ItemDataAsset : ItemDataAssets)
	{
		if (ItemDataAsset && ItemDataAsset->ItemId == ItemId)
		{
			return ItemDataAsset;
		}
	}

	if (UPRAssetManager* AssetManager = UPRAssetManager::Get())
	{
		return AssetManager->LoadItemDataSync(ItemId);
	}

	return nullptr;
}

int32 UPRInventoryComponent::GetMaxStackCount(const FName ItemId) const
{
	if (const UPRItemDataAsset* ItemDataAsset = FindItemData(ItemId))
	{
		return FMath::Max(1, ItemDataAsset->MaxStackCount);
	}

	return TNumericLimits<int32>::Max();
}

int32 UPRInventoryComponent::FindItemIndex(const FName ItemId) const
{
	return InventoryList.FindEntryIndex(ItemId);
}

int32 UPRInventoryComponent::FindStackableEntryIndex(const FPRItemInstance& Item) const
{
	const int32 MaxStackCount = GetMaxStackCount(Item.ItemId);
	return InventoryList.Entries.IndexOfByPredicate(
		[&Item, MaxStackCount](const FPRInventoryEntry& Entry)
		{
			return CanStackItemInstances(Entry.Item, Item) && Entry.Item.Count < MaxStackCount;
		});
}

void UPRInventoryComponent::HandleInventoryListChanged()
{
	RefreshCachedItems();
	OnInventoryChanged.Broadcast();
}

void UPRInventoryComponent::NotifyInventoryChanged()
{
	HandleInventoryListChanged();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UPRInventoryComponent::RefreshCachedItems()
{
	CachedItems.Reset(InventoryList.Entries.Num());
	for (const FPRInventoryEntry& Entry : InventoryList.Entries)
	{
		CachedItems.Add(Entry.Item);
	}
}

bool UPRInventoryComponent::CanStackItemInstances(const FPRItemInstance& ExistingItem, const FPRItemInstance& IncomingItem)
{
	return ExistingItem.HasEquivalentStackingState(IncomingItem);
}
