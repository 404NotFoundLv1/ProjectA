#include "UI/PRInventoryViewModel.h"

#include "Items/PREquipmentComponent.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTransactionComponent.h"
#include "Items/PRQuickbarComponent.h"
#include "Items/PRWarehouseComponent.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"
#include "Items/PRWeaponDataAsset.h"

void UPRInventoryViewModel::BindPlayerState(APRPlayerState* InPlayerState)
{
	if (BoundPlayerState.Get() == InPlayerState)
	{
		Refresh();
		return;
	}

	UnbindPlayerState();
	BoundPlayerState = InPlayerState;
	BoundInventory = InPlayerState ? InPlayerState->GetInventoryComponent() : nullptr;
	BoundWarehouse = InPlayerState ? InPlayerState->GetWarehouseComponent() : nullptr;
	BoundEquipment = InPlayerState ? InPlayerState->GetEquipmentComponent() : nullptr;
	BoundWeapon = InPlayerState ? InPlayerState->GetWeaponComponent() : nullptr;
	BoundQuickbar = InPlayerState ? InPlayerState->GetQuickbarComponent() : nullptr;
	BoundTransactions = InPlayerState ? InPlayerState->GetItemTransactionComponent() : nullptr;

	if (UPRInventoryComponent* Inventory = BoundInventory.Get()) { Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRWarehouseComponent* Warehouse = BoundWarehouse.Get()) { Warehouse->OnWarehouseChanged.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPREquipmentComponent* Equipment = BoundEquipment.Get()) { Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRWeaponComponent* Weapon = BoundWeapon.Get()) { Weapon->OnWeaponStateChanged.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRQuickbarComponent* Quickbar = BoundQuickbar.Get()) { Quickbar->OnQuickbarChanged.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRItemTransactionComponent* Transactions = BoundTransactions.Get()) { Transactions->OnTransactionResult.AddUniqueDynamic(this, &UPRInventoryViewModel::HandleTransactionResult); }
	Refresh();
}

void UPRInventoryViewModel::UnbindPlayerState()
{
	if (UPRInventoryComponent* Inventory = BoundInventory.Get()) { Inventory->OnInventoryChanged.RemoveDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRWarehouseComponent* Warehouse = BoundWarehouse.Get()) { Warehouse->OnWarehouseChanged.RemoveDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPREquipmentComponent* Equipment = BoundEquipment.Get()) { Equipment->OnEquipmentChanged.RemoveDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRWeaponComponent* Weapon = BoundWeapon.Get()) { Weapon->OnWeaponStateChanged.RemoveDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRQuickbarComponent* Quickbar = BoundQuickbar.Get()) { Quickbar->OnQuickbarChanged.RemoveDynamic(this, &UPRInventoryViewModel::HandleAuthoritativeStateChanged); }
	if (UPRItemTransactionComponent* Transactions = BoundTransactions.Get()) { Transactions->OnTransactionResult.RemoveDynamic(this, &UPRInventoryViewModel::HandleTransactionResult); }
	BoundPlayerState.Reset(); BoundInventory.Reset(); BoundWarehouse.Reset(); BoundEquipment.Reset(); BoundWeapon.Reset(); BoundQuickbar.Reset(); BoundTransactions.Reset();
	FilteredRows.Reset(); SelectedInstanceGuid.Invalidate(); PageIndex = 0;
}

void UPRInventoryViewModel::SetSource(const EPRInventoryPresentationSource InSource) { if (Source != InSource) { Source = InSource; PageIndex = 0; Refresh(); } }
void UPRInventoryViewModel::SetSort(const EPRInventorySortField InSortField, const bool bInAscending) { if (SortField != InSortField || bAscending != bInAscending) { SortField = InSortField; bAscending = bInAscending; Refresh(); } }
void UPRInventoryViewModel::SetFilter(const EPRInventoryFilter InFilter, const FString& InSearchText) { const FString Trimmed = InSearchText.TrimStartAndEnd(); if (Filter != InFilter || SearchText != Trimmed) { Filter = InFilter; SearchText = Trimmed; PageIndex = 0; Refresh(); } }
void UPRInventoryViewModel::SetPageIndex(const int32 InPageIndex) { PageIndex = FMath::Max(0, InPageIndex); NormalizePageAndSelection(); OnViewChanged.Broadcast(); }
void UPRInventoryViewModel::SelectInstance(const FGuid InstanceGuid) { SelectedInstanceGuid = InstanceGuid; NormalizePageAndSelection(); OnViewChanged.Broadcast(); }

void UPRInventoryViewModel::SelectNextFocusable(const int32 Direction)
{
	if (FilteredRows.IsEmpty()) { SelectedInstanceGuid.Invalidate(); OnViewChanged.Broadcast(); return; }
	int32 SelectedIndex = FilteredRows.IndexOfByPredicate([this](const FPRInventoryPresentationRow& Row) { return Row.Item.InstanceGuid == SelectedInstanceGuid; });
	if (SelectedIndex == INDEX_NONE) { SelectedIndex = Direction < 0 ? FilteredRows.Num() - 1 : 0; }
	else { SelectedIndex = (SelectedIndex + (Direction < 0 ? -1 : 1) + FilteredRows.Num()) % FilteredRows.Num(); }
	SelectedInstanceGuid = FilteredRows[SelectedIndex].Item.InstanceGuid;
	PageIndex = SelectedIndex / PageSize;
	OnViewChanged.Broadcast();
}

void UPRInventoryViewModel::Refresh()
{
	FilteredRows.Reset();
	const TArray<FPRItemInstance> Items = Source == EPRInventoryPresentationSource::Backpack
		? (BoundInventory.IsValid() ? BoundInventory->GetInventoryItems() : TArray<FPRItemInstance>())
		: (BoundWarehouse.IsValid() ? BoundWarehouse->GetWarehouseItems() : TArray<FPRItemInstance>());
	for (const FPRItemInstance& Item : Items)
	{
		if (PassesFilter(Item)) { FilteredRows.Add(BuildRow(Item)); }
	}
	if (SortField != EPRInventorySortField::AuthorityOrder)
	{
		FilteredRows.StableSort([this](const FPRInventoryPresentationRow& Left, const FPRInventoryPresentationRow& Right)
		{
			int32 Compare = 0;
			switch (SortField)
			{
			case EPRInventorySortField::Name: Compare = GetDisplayName(Left.Item).ToString().Compare(GetDisplayName(Right.Item).ToString(), ESearchCase::IgnoreCase); break;
			case EPRInventorySortField::Type:
			{
				const UPRItemDataAsset* LeftData = BoundInventory.IsValid() ? BoundInventory->FindItemData(Left.Item.ItemId) : nullptr;
				const UPRItemDataAsset* RightData = BoundInventory.IsValid() ? BoundInventory->FindItemData(Right.Item.ItemId) : nullptr;
				Compare = static_cast<int32>(LeftData ? LeftData->ItemType : EPRItemType::QuestItem) - static_cast<int32>(RightData ? RightData->ItemType : EPRItemType::QuestItem);
				break;
			}
			case EPRInventorySortField::Rarity: Compare = static_cast<int32>(Left.Item.Rarity) - static_cast<int32>(Right.Item.Rarity); break;
			case EPRInventorySortField::Level: Compare = Left.Item.Level - Right.Item.Level; break;
			default: break;
			}
			if (Compare == 0) { Compare = Left.Item.InstanceGuid.ToString(EGuidFormats::Digits).Compare(Right.Item.InstanceGuid.ToString(EGuidFormats::Digits)); }
			return bAscending ? Compare < 0 : Compare > 0;
		});
	}
	NormalizePageAndSelection();
	OnViewChanged.Broadcast();
}

TArray<FPRInventoryPresentationRow> UPRInventoryViewModel::GetVisibleRows() const
{
	const int32 First = PageIndex * PageSize;
	const int32 Count = FMath::Clamp(FilteredRows.Num() - First, 0, PageSize);
	return Count > 0 ? TArray<FPRInventoryPresentationRow>(FilteredRows.GetData() + First, Count) : TArray<FPRInventoryPresentationRow>();
}

FPRInventoryPageState UPRInventoryViewModel::GetPageState() const
{
	FPRInventoryPageState State; State.PageIndex = PageIndex; State.TotalRows = FilteredRows.Num(); State.PageSize = PageSize; State.PageCount = FMath::Max(1, FMath::DivideAndRoundUp(FilteredRows.Num(), PageSize)); return State;
}

FPRInventoryPresentationRow UPRInventoryViewModel::GetSelectedRow() const
{
	const FPRInventoryPresentationRow* Row = FilteredRows.FindByPredicate([this](const FPRInventoryPresentationRow& Candidate) { return Candidate.Item.InstanceGuid == SelectedInstanceGuid; });
	return Row ? *Row : FPRInventoryPresentationRow();
}

FPRInventoryComparison UPRInventoryViewModel::GetSelectedComparison() const
{
	FPRInventoryComparison Result; Result.Candidate = GetSelectedRow().Item;
	if (!Result.Candidate.IsValid() || !BoundInventory.IsValid()) { return Result; }
	const UPRItemDataAsset* Data = BoundInventory->FindItemData(Result.Candidate.ItemId);
	const UPREquipmentDataAsset* EquipmentData = Cast<UPREquipmentDataAsset>(Data);
	FName SlotId = NAME_None;
	if (EquipmentData) { SlotId = UPREquipmentComponent::GetSlotId(EquipmentData->EquipmentSlot); }
	else if (Cast<UPRWeaponDataAsset>(Data)) { SlotId = ProjectRiftEquipmentSlots::Weapon; }
	if (SlotId.IsNone()) { return Result; }
	if (SlotId == ProjectRiftEquipmentSlots::Weapon && BoundWeapon.IsValid()) { Result.Equipped = BoundWeapon->GetEquippedWeapon(); }
	else if (BoundEquipment.IsValid()) { Result.Equipped = BoundEquipment->GetEquippedItem(SlotId); }
	Result.Differences.Add(FText::FromString(FString::Printf(TEXT("Level %+d"), Result.Candidate.Level - Result.Equipped.Level)));
	Result.Differences.Add(FText::FromString(FString::Printf(TEXT("Rarity %+d"), static_cast<int32>(Result.Candidate.Rarity) - static_cast<int32>(Result.Equipped.Rarity))));
	Result.Differences.Add(FText::FromString(FString::Printf(TEXT("Affixes %+d"), Result.Candidate.RolledAffixes.Num() - Result.Equipped.RolledAffixes.Num())));
	return Result;
}

void UPRInventoryViewModel::HandleAuthoritativeStateChanged() { Refresh(); }
void UPRInventoryViewModel::HandleTransactionResult(const FPRItemTransactionResult& Result) { LastTransactionResult = Result; OnTransactionResult.Broadcast(Result); Refresh(); }

bool UPRInventoryViewModel::PassesFilter(const FPRItemInstance& Item) const
{
	const UPRItemDataAsset* Data = BoundInventory.IsValid() ? BoundInventory->FindItemData(Item.ItemId) : nullptr;
	if (!SearchText.IsEmpty() && !GetDisplayName(Item).ToString().Contains(SearchText, ESearchCase::IgnoreCase)) { return false; }
	if (Filter == EPRInventoryFilter::All) { return true; }
	if (Filter == EPRInventoryFilter::Equipment) { return Data && Data->bCanEquip; }
	if (Filter == EPRInventoryFilter::Consumable) { return Data && Data->ItemType == EPRItemType::Consumable; }
	if (Filter == EPRInventoryFilter::Ammunition) { return Data && Data->ItemType == EPRItemType::Ammunition; }
	const UPREquipmentDataAsset* EquipmentData = Cast<UPREquipmentDataAsset>(Data);
	return EquipmentData && EquipmentData->EquipmentSlot == EPREquipmentSlot::Tool;
}

FPRInventoryPresentationRow UPRInventoryViewModel::BuildRow(const FPRItemInstance& Item) const
{
	FPRInventoryPresentationRow Row; Row.Item = Item; Row.DisplayName = GetDisplayName(Item); Row.bFromWarehouse = Source == EPRInventoryPresentationSource::Warehouse;
	const UPRItemDataAsset* Data = BoundInventory.IsValid() ? BoundInventory->FindItemData(Item.ItemId) : nullptr;
	Row.Icon = Data ? Data->Icon : nullptr; Row.bEquippable = Data && Data->bCanEquip; Row.bUsable = !Row.bFromWarehouse && Data && Data->bCanUse; Row.bDroppable = !Row.bFromWarehouse && Data && Data->bCanDrop; Row.QuickbarSlotIndex = !Row.bFromWarehouse ? GetQuickbarSlotIndex(Item.InstanceGuid) : INDEX_NONE;
	return Row;
}

FText UPRInventoryViewModel::GetDisplayName(const FPRItemInstance& Item) const
{
	const UPRItemDataAsset* Data = BoundInventory.IsValid() ? BoundInventory->FindItemData(Item.ItemId) : nullptr;
	return Data && !Data->DisplayName.IsEmpty() ? Data->DisplayName : FText::FromString(Item.ItemId.IsNone() ? TEXT("Unknown Item") : FName::NameToDisplayString(Item.ItemId.ToString(), false));
}

int32 UPRInventoryViewModel::GetQuickbarSlotIndex(const FGuid& InstanceGuid) const
{
	if (!BoundQuickbar.IsValid()) { return INDEX_NONE; }
	const FPRQuickSlotReference* Slot = BoundQuickbar->GetQuickSlots().FindByPredicate([&InstanceGuid](const FPRQuickSlotReference& Candidate) { return Candidate.InstanceGuid == InstanceGuid; });
	return Slot ? Slot->SlotIndex : INDEX_NONE;
}

void UPRInventoryViewModel::NormalizePageAndSelection()
{
	const int32 PageCount = FMath::Max(1, FMath::DivideAndRoundUp(FilteredRows.Num(), PageSize)); PageIndex = FMath::Clamp(PageIndex, 0, PageCount - 1);
	if (!SelectedInstanceGuid.IsValid() || !FilteredRows.ContainsByPredicate([this](const FPRInventoryPresentationRow& Row) { return Row.Item.InstanceGuid == SelectedInstanceGuid; }))
	{
		const int32 First = PageIndex * PageSize; SelectedInstanceGuid = FilteredRows.IsValidIndex(First) ? FilteredRows[First].Item.InstanceGuid : FGuid();
	}
}
