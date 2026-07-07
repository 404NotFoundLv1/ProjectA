#include "Items/PRInventoryComponent.h"

#include "GameFramework/Actor.h"
#include "ProjectA.h"

UPRInventoryComponent::UPRInventoryComponent()
	: MaxSlots(20)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPRInventoryComponent::CanAddItem(const FPRItemInstance& Item) const
{
	if (!Item.IsValid() || MaxSlots <= 0)
	{
		return false;
	}

	return FindItemIndex(Item.ItemId) != INDEX_NONE || Items.Num() < MaxSlots;
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
			Items.Num(),
			MaxSlots);
		return false;
	}

	const int32 ExistingIndex = FindItemIndex(Item.ItemId);
	if (ExistingIndex != INDEX_NONE)
	{
		Items[ExistingIndex].Count += Item.Count;
	}
	else
	{
		Items.Add(Item);
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
		Items.Num(),
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
	if (ExistingIndex == INDEX_NONE || Items[ExistingIndex].Count < Count)
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory remove rejected. Owner=%s ItemId=%s Requested=%d Available=%d"),
			*GetNameSafe(GetOwner()),
			*ItemId.ToString(),
			Count,
			ExistingIndex == INDEX_NONE ? 0 : Items[ExistingIndex].Count);
		return false;
	}

	Items[ExistingIndex].Count -= Count;
	if (Items[ExistingIndex].Count <= 0)
	{
		Items.RemoveAt(ExistingIndex);
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
		Items.Num(),
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
	return ExistingIndex == INDEX_NONE ? 0 : Items[ExistingIndex].Count;
}

int32 UPRInventoryComponent::FindItemIndex(const FName ItemId) const
{
	return Items.IndexOfByPredicate(
		[ItemId](const FPRItemInstance& Item)
		{
			return Item.ItemId == ItemId;
		});
}

void UPRInventoryComponent::NotifyInventoryChanged()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}
