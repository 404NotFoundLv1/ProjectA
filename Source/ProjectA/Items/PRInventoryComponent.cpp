#include "Items/PRInventoryComponent.h"

#include "GameFramework/Actor.h"
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

	return FindItemIndex(Item.ItemId) != INDEX_NONE || InventoryList.Entries.Num() < MaxSlots;
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

	const int32 ExistingIndex = FindItemIndex(Item.ItemId);
	if (ExistingIndex != INDEX_NONE)
	{
		FPRInventoryEntry& ExistingEntry = InventoryList.Entries[ExistingIndex];
		ExistingEntry.Item.Count += Item.Count;
		InventoryList.MarkItemDirty(ExistingEntry);
	}
	else
	{
		FPRInventoryEntry& NewEntry = InventoryList.Entries.AddDefaulted_GetRef();
		NewEntry.Item = Item;
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

	const int32 ExistingIndex = FindItemIndex(ItemId);
	if (ExistingIndex == INDEX_NONE || InventoryList.Entries[ExistingIndex].Item.Count < Count)
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory remove rejected. Owner=%s ItemId=%s Requested=%d Available=%d"),
			*GetNameSafe(GetOwner()),
			*ItemId.ToString(),
			Count,
			ExistingIndex == INDEX_NONE ? 0 : InventoryList.Entries[ExistingIndex].Item.Count);
		return false;
	}

	FPRInventoryEntry& ExistingEntry = InventoryList.Entries[ExistingIndex];
	ExistingEntry.Item.Count -= Count;
	if (ExistingEntry.Item.Count <= 0)
	{
		InventoryList.Entries.RemoveAt(ExistingIndex);
		InventoryList.MarkArrayDirty();
	}
	else
	{
		InventoryList.MarkItemDirty(ExistingEntry);
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

int32 UPRInventoryComponent::GetItemCount(const FName ItemId) const
{
	const int32 ExistingIndex = FindItemIndex(ItemId);
	return ExistingIndex == INDEX_NONE ? 0 : InventoryList.Entries[ExistingIndex].Item.Count;
}

int32 UPRInventoryComponent::FindItemIndex(const FName ItemId) const
{
	return InventoryList.FindEntryIndex(ItemId);
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
