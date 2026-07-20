#include "UI/PRInventoryWidget.h"

#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Core/PRAssetManager.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRQuickbarComponent.h"
#include "Items/PREquipmentComponent.h"
#include "Items/PRWeaponDataAsset.h"
#include "UI/PRInventoryViewModel.h"
#include "Misc/Paths.h"
#include "Player/PRPlayerState.h"
#include "Player/PRPlayerController.h"
#include "Weapons/PRWeaponComponent.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FSlateFontInfo GetProjectRiftInventoryFont(const float Size)
{
	static const TSharedPtr<const FCompositeFont> InventoryFont = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"),
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
		EFontHinting::Default,
		EFontLoadingPolicy::LazyLoad);

	return FSlateFontInfo(InventoryFont, Size);
}
}

void UPRInventoryWidget::BindInventory(UPRInventoryComponent* InInventoryComponent)
{
	if (UPRWeaponComponent* CurrentWeapon = GetBoundWeaponComponent())
	{
		CurrentWeapon->OnWeaponStateChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleWeaponStateChanged);
	}
	if (UPREquipmentComponent* CurrentEquipment = GetBoundEquipmentComponent())
	{
		CurrentEquipment->OnEquipmentChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleEquipmentChanged);
	}
	if (UPRInventoryComponent* CurrentInventory = BoundInventory.Get())
	{
		CurrentInventory->OnInventoryChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = InInventoryComponent;
	SelectedItemIndex = INDEX_NONE;

	if (InInventoryComponent)
	{
		InInventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &UPRInventoryWidget::HandleInventoryChanged);
		if (UPRWeaponComponent* Weapon = GetBoundWeaponComponent())
		{
			Weapon->OnWeaponStateChanged.AddUniqueDynamic(this, &UPRInventoryWidget::HandleWeaponStateChanged);
		}
		if (UPREquipmentComponent* Equipment = GetBoundEquipmentComponent())
		{
			Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UPRInventoryWidget::HandleEquipmentChanged);
		}
	}

	RefreshInventory();
}

void UPRInventoryWidget::BindViewModel(UPRInventoryViewModel* InViewModel)
{
	if (UPRInventoryViewModel* Current = BoundViewModel.Get())
	{
		Current->OnViewChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleViewModelChanged);
	}
	BoundViewModel = InViewModel;
	if (InViewModel)
	{
		InViewModel->OnViewChanged.AddUniqueDynamic(this, &UPRInventoryWidget::HandleViewModelChanged);
	}
	RefreshInventory();
}

void UPRInventoryWidget::RefreshInventory()
{
	DisplayedItems.Reset();

	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		for (const FPRInventoryPresentationRow& Row : ViewModel->GetVisibleRows())
		{
			DisplayedItems.Add(Row.Item);
		}
	}
	else if (UPRInventoryComponent* InventoryComponent = BoundInventory.Get())
	{
		DisplayedItems = InventoryComponent->GetInventoryItems();
	}

	if (!DisplayedItems.IsValidIndex(SelectedItemIndex))
	{
		SelectedItemIndex = INDEX_NONE;
	}
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		const FGuid SelectedGuid = ViewModel->GetSelectedInstanceGuid();
		SelectedItemIndex = DisplayedItems.IndexOfByPredicate([SelectedGuid](const FPRItemInstance& Item)
		{
			return Item.InstanceGuid == SelectedGuid;
		});
	}

	RebuildItemList();
	RefreshSelectedItemDetails();
	RefreshShipResourceSummary();
	RefreshPrimaryWeaponSummary();
}

void UPRInventoryWidget::SelectDisplayedItem(const int32 ItemIndex)
{
	SelectedItemIndex = DisplayedItems.IsValidIndex(ItemIndex) ? ItemIndex : INDEX_NONE;
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		ViewModel->SelectInstance(HasSelectedItem() ? DisplayedItems[SelectedItemIndex].InstanceGuid : FGuid());
	}
	RebuildItemList();
	RefreshSelectedItemDetails();
}

bool UPRInventoryWidget::HasSelectedItem() const
{
	return DisplayedItems.IsValidIndex(SelectedItemIndex);
}

FPRItemInstance UPRInventoryWidget::GetSelectedItem() const
{
	return HasSelectedItem() ? DisplayedItems[SelectedItemIndex] : FPRItemInstance();
}

FText UPRInventoryWidget::GetItemDisplayName(const FPRItemInstance& Item) const
{
	if (const UPRItemDataAsset* ItemData = FindItemData(Item))
	{
		if (!ItemData->DisplayName.IsEmpty())
		{
			return ItemData->DisplayName;
		}
	}

	if (Item.ItemId.IsNone())
	{
		return FText::FromString(TEXT("Unknown Item"));
	}

	return FText::FromString(FName::NameToDisplayString(Item.ItemId.ToString(), false));
}

UTexture2D* UPRInventoryWidget::GetItemIcon(const FPRItemInstance& Item) const
{
	const UPRItemDataAsset* ItemData = FindItemData(Item);
	return ItemData ? ItemData->Icon : nullptr;
}

FText UPRInventoryWidget::GetItemRarityText(const EPRItemRarity Rarity) const
{
	switch (Rarity)
	{
	case EPRItemRarity::Uncommon:
		return FText::FromString(TEXT("Uncommon"));
	case EPRItemRarity::Rare:
		return FText::FromString(TEXT("Rare"));
	case EPRItemRarity::Epic:
		return FText::FromString(TEXT("Epic"));
	case EPRItemRarity::Legendary:
		return FText::FromString(TEXT("Legendary"));
	case EPRItemRarity::Prototype:
		return FText::FromString(TEXT("Prototype"));
	case EPRItemRarity::Common:
	default:
		return FText::FromString(TEXT("Common"));
	}
}

FLinearColor UPRInventoryWidget::GetItemRarityColor(const EPRItemRarity Rarity) const
{
	switch (Rarity)
	{
	case EPRItemRarity::Uncommon:
		return FLinearColor(0.42f, 0.82f, 0.48f, 1.0f);
	case EPRItemRarity::Rare:
		return FLinearColor(0.35f, 0.62f, 1.0f, 1.0f);
	case EPRItemRarity::Epic:
		return FLinearColor(0.74f, 0.48f, 0.95f, 1.0f);
	case EPRItemRarity::Legendary:
		return FLinearColor(1.0f, 0.74f, 0.28f, 1.0f);
	case EPRItemRarity::Prototype:
		return FLinearColor(1.0f, 0.38f, 0.72f, 1.0f);
	case EPRItemRarity::Common:
	default:
		return FLinearColor(0.86f, 0.88f, 0.84f, 1.0f);
	}
}

FText UPRInventoryWidget::GetItemTooltipText(const FPRItemInstance& Item) const
{
	TArray<FString> Lines;
	Lines.Add(GetItemDisplayName(Item).ToString());
	Lines.Add(FString::Printf(TEXT("Rarity: %s"), *GetItemRarityText(Item.Rarity).ToString()));
	Lines.Add(FString::Printf(TEXT("Count: %d"), Item.Count));
	Lines.Add(FString::Printf(TEXT("Level: %d"), Item.Level));
	Lines.Add(FString::Printf(TEXT("Durability: %.0f%%"), FMath::Clamp(Item.Durability, 0.0f, 1.0f) * 100.0f));

	if (const UPRItemDataAsset* ItemData = FindItemData(Item))
	{
		if (!ItemData->Description.IsEmpty())
		{
			Lines.Add(ItemData->Description.ToString());
		}
	}

	if (!Item.RolledAffixes.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Loot seed: %d"), Item.LootSeed));
		for (const FPRAffixRoll& Roll : Item.RolledAffixes)
		{
			const UPRAffixDefinitionDataAsset* Affix = UPRAssetManager::Get()
				? UPRAssetManager::Get()->LoadAffixSync(Roll.AffixId)
				: nullptr;
			const FString Label = Affix && !Affix->DisplayName.IsEmpty() ? Affix->DisplayName.ToString() : Roll.AffixId.ToString();
			const bool bPercent = Roll.ModifierType == EPRAffixModifierType::Percentage || Roll.Attribute == EPRAffixAttribute::PollutionResistance;
			const float DisplayMagnitude = bPercent ? Roll.Magnitude * 100.0f : Roll.Magnitude;
			Lines.Add(FString::Printf(TEXT("%s %+0.2f%s"), *Label, DisplayMagnitude, bPercent ? TEXT("%") : TEXT("")));
			if (Affix && !Affix->Description.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("  %s"), *Affix->Description.ToString()));
			}
		}
	}
	else if (!Item.Affixes.IsEmpty())
	{
		TArray<FString> AffixNames;
		AffixNames.Reserve(Item.Affixes.Num());
		for (const FName Affix : Item.Affixes)
		{
			AffixNames.Add(Affix.ToString());
		}

		Lines.Add(FString::Printf(TEXT("Affixes: %s"), *FString::Join(AffixNames, TEXT(", "))));
	}

	return FText::FromString(FString::Join(Lines, LINE_TERMINATOR));
}

FText UPRInventoryWidget::GetShipResourceSummaryText() const
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	const APRPlayerState* ProjectRiftPlayerState = InventoryComponent ? Cast<APRPlayerState>(InventoryComponent->GetOwner()) : nullptr;
	const FString ResourceSummary = ProjectRiftPlayerState ? ProjectRiftPlayerState->BuildShipResourceSummary() : FString(TEXT("None"));
	return FText::FromString(FString::Printf(TEXT("\u8230\u8239\u8D44\u6E90\uFF1A%s"), *ResourceSummary));
}

FText UPRInventoryWidget::GetPrimaryWeaponSummaryText() const
{
	const UPRWeaponComponent* Weapon = GetBoundWeaponComponent();
	const UPRWeaponDataAsset* WeaponData = Weapon ? Weapon->GetEquippedWeaponData() : nullptr;
	if (!Weapon || !WeaponData)
	{
		return FText::FromString(TEXT("Primary: Empty"));
	}

	const FString WeaponName = WeaponData->DisplayName.IsEmpty()
		? WeaponData->ItemId.ToString()
		: WeaponData->DisplayName.ToString();
	return FText::FromString(FString::Printf(
		TEXT("Primary: %s    %d / %d"),
		*WeaponName,
		Weapon->GetMagazineAmmo(),
		Weapon->GetReserveAmmo()));
}

FText UPRInventoryWidget::GetEquipmentSummaryText() const
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	const APRPlayerState* PlayerState = InventoryComponent ? Cast<APRPlayerState>(InventoryComponent->GetOwner()) : nullptr;
	const UPREquipmentComponent* EquipmentComponent = PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{
		return FText::FromString(TEXT("Armor: Empty    Chip: Empty    Tool: Empty"));
	}
	auto GetSlotName = [EquipmentComponent](const FName SlotId)
	{
		const FPRItemInstance Item = EquipmentComponent->GetEquippedItem(SlotId);
		return Item.IsValid() ? Item.ItemId.ToString() : FString(TEXT("Empty"));
	};
	return FText::FromString(FString::Printf(TEXT("Armor: %s    Chip: %s    Tool: %s"),
		*GetSlotName(ProjectRiftEquipmentSlots::Armor),
		*GetSlotName(ProjectRiftEquipmentSlots::Chip),
		*GetSlotName(ProjectRiftEquipmentSlots::Tool)));
}

FText UPRInventoryWidget::GetQuickbarSummaryText() const
{
	const APRPlayerState* PlayerState = BoundInventory.IsValid() ? Cast<APRPlayerState>(BoundInventory->GetOwner()) : nullptr;
	const UPRQuickbarComponent* Quickbar = PlayerState ? PlayerState->GetQuickbarComponent() : nullptr;
	if (!Quickbar)
	{
		return FText::FromString(TEXT("Quickbar: Unavailable"));
	}
	return FText::FromString(FString::Printf(TEXT("Quickbar: %d / 4%s"), Quickbar->GetQuickSlots().Num(), Quickbar->IsUsingItem() ? TEXT("  USING") : TEXT("")));
}

void UPRInventoryWidget::RequestUseSelectedItem()
{
	if (!IsPresentingWarehouse() && HasSelectedItem())
	{
		OnUseItemRequested.Broadcast(DisplayedItems[SelectedItemIndex].ItemId);
	}
}

void UPRInventoryWidget::RequestUseDisplayedItem(const int32 ItemIndex)
{
	if (!IsPresentingWarehouse() && DisplayedItems.IsValidIndex(ItemIndex))
	{
		SelectedItemIndex = ItemIndex;
		OnUseItemRequested.Broadcast(DisplayedItems[ItemIndex].ItemId);
		RebuildItemList();
		RefreshSelectedItemDetails();
	}
}

void UPRInventoryWidget::RequestAssignSelectedItemToQuickbar(const int32 SlotIndex)
{
	if (!IsPresentingWarehouse() && HasSelectedItem() && SlotIndex >= 0 && SlotIndex < 4)
	{
		OnQuickbarAssignRequested.Broadcast(SlotIndex, DisplayedItems[SelectedItemIndex].InstanceGuid);
	}
}

void UPRInventoryWidget::RequestClearQuickbarSlot(const int32 SlotIndex)
{
	if (SlotIndex >= 0 && SlotIndex < 4)
	{
		OnQuickbarClearRequested.Broadcast(SlotIndex);
	}
}

void UPRInventoryWidget::RequestDropSelectedItem(const int32 Count)
{
	if (!IsPresentingWarehouse() && HasSelectedItem() && Count > 0)
	{
		OnDropItemRequested.Broadcast(DisplayedItems[SelectedItemIndex].ItemId, Count);
	}
}

void UPRInventoryWidget::RequestDropDisplayedItem(const int32 ItemIndex, const int32 Count)
{
	if (!IsPresentingWarehouse() && DisplayedItems.IsValidIndex(ItemIndex) && Count > 0)
	{
		SelectedItemIndex = ItemIndex;
		OnDropItemRequested.Broadcast(DisplayedItems[ItemIndex].ItemId, Count);
		RebuildItemList();
		RefreshSelectedItemDetails();
	}
}

void UPRInventoryWidget::RequestEquipSelectedItem()
{
	if (IsPresentingWarehouse() || !HasSelectedItem())
	{
		return;
	}

	const FPRItemInstance& Item = DisplayedItems[SelectedItemIndex];
	const UPRItemDataAsset* ItemData = FindItemData(Item);
	if (ItemData && ItemData->bCanEquip)
	{
		OnEquipItemRequested.Broadcast(Item.ItemId);
	}
}

void UPRInventoryWidget::RequestUnequipPrimaryWeapon()
{
	const UPRWeaponComponent* Weapon = GetBoundWeaponComponent();
	if (Weapon && Weapon->GetEquippedWeapon().IsValid())
	{
		OnUnequipPrimaryRequested.Broadcast();
	}
}

void UPRInventoryWidget::RequestUnequipEquipmentSlot(const FName SlotId)
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	const APRPlayerState* PlayerState = InventoryComponent ? Cast<APRPlayerState>(InventoryComponent->GetOwner()) : nullptr;
	const UPREquipmentComponent* EquipmentComponent = PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent && EquipmentComponent->GetEquippedItem(SlotId).IsValid())
	{
		OnUnequipEquipmentRequested.Broadcast(SlotId);
	}
}

void UPRInventoryWidget::ShowBackpackPresentation()
{
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		ViewModel->SetSource(EPRInventoryPresentationSource::Backpack);
	}
}

void UPRInventoryWidget::ShowWarehousePresentation()
{
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get(); ViewModel && Cast<APRPlayerController>(GetOwningPlayer())
		&& Cast<APRPlayerController>(GetOwningPlayer())->IsShipWarehouseAvailable())
	{
		ViewModel->SetSource(EPRInventoryPresentationSource::Warehouse);
	}
}

void UPRInventoryWidget::ChangePresentationPage(const int32 Delta)
{
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		ViewModel->ChangePage(Delta);
	}
}

void UPRInventoryWidget::CyclePresentationSort()
{
	UPRInventoryViewModel* ViewModel = BoundViewModel.Get();
	if (!ViewModel)
	{
		return;
	}
	const int32 Current = static_cast<int32>(ViewModel->GetSortField());
	const int32 Next = (Current + 1) % (static_cast<int32>(EPRInventorySortField::Level) + 1);
	ViewModel->SetSort(static_cast<EPRInventorySortField>(Next), true);
}

void UPRInventoryWidget::CyclePresentationFilter()
{
	UPRInventoryViewModel* ViewModel = BoundViewModel.Get();
	if (!ViewModel)
	{
		return;
	}
	const int32 Current = static_cast<int32>(ViewModel->GetFilter());
	const int32 Next = (Current + 1) % (static_cast<int32>(EPRInventoryFilter::Ammunition) + 1);
	ViewModel->SetFilter(static_cast<EPRInventoryFilter>(Next), FString());
}

void UPRInventoryWidget::MovePresentationFocus(const int32 Direction)
{
	if (UPRInventoryViewModel* ViewModel = BoundViewModel.Get())
	{
		ViewModel->SelectNextFocusable(Direction);
	}
}

void UPRInventoryWidget::RequestTransferSelectedItem(const int32 Count)
{
	if (!HasSelectedItem() || Count <= 0)
	{
		return;
	}
	APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	UPRInventoryViewModel* ViewModel = BoundViewModel.Get();
	if (!Controller || !ViewModel || !Controller->IsShipWarehouseAvailable())
	{
		return;
	}
	if (ViewModel->GetSource() == EPRInventoryPresentationSource::Backpack)
	{
		Controller->StoreInventoryInstanceInWarehouse(GetSelectedItem().InstanceGuid, Count);
	}
	else
	{
		Controller->RetrieveWarehouseInstance(GetSelectedItem().InstanceGuid, Count);
	}
}

void UPRInventoryWidget::BeginDropConfirmationSelectedItem(const int32 Count)
{
	PendingConfirmation = FPRInventoryConfirmationRequest();
	if (IsPresentingWarehouse() || !HasSelectedItem() || Count <= 0)
	{
		return;
	}
	const FPRItemInstance Item = GetSelectedItem();
	if (Count > Item.Count)
	{
		return;
	}
	PendingConfirmation.Action = EPRInventoryConfirmationAction::Drop;
	PendingConfirmation.Item = Item;
	PendingConfirmation.Count = Count;
	PendingConfirmation.Prompt = FText::FromString(FString::Printf(TEXT("Drop %d x %s?"), Count, *GetItemDisplayName(Item).ToString()));
	OnConfirmationRequested.Broadcast(PendingConfirmation);
	RefreshSelectedItemDetails();
}

void UPRInventoryWidget::ConfirmPendingInventoryAction(const bool bAccepted)
{
	const FPRInventoryConfirmationRequest Confirmed = PendingConfirmation;
	PendingConfirmation = FPRInventoryConfirmationRequest();
	if (bAccepted && Confirmed.IsValid() && Confirmed.Action == EPRInventoryConfirmationAction::Drop && !IsPresentingWarehouse())
	{
		OnDropItemRequested.Broadcast(Confirmed.Item.ItemId, Confirmed.Count);
	}
	RefreshSelectedItemDetails();
}

TSharedRef<SWidget> UPRInventoryWidget::RebuildWidget()
{
	TSharedRef<SWidget> Widget = SNew(SBorder)
		.Padding(FMargin(14.0f))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.9f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 0.92f, 1.0f)))
				.Font(GetProjectRiftInventoryFont(18.0f))
				.Text(FText::FromString(TEXT("Inventory")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleShowBackpackClicked))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("Backpack")))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled_Lambda([this]()
					{
						const APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
						return Controller && Controller->IsShipWarehouseAvailable();
					})
					.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleShowWarehouseClicked))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("Ship Warehouse")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandlePreviousPageClicked))
					[
						SNew(STextBlock).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("<")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleCycleSortClicked))
					[
						SNew(STextBlock).Font(GetProjectRiftInventoryFont(10.0f)).Text(FText::FromString(TEXT("Sort")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleCycleFilterClicked))
					[
						SNew(STextBlock).Font(GetProjectRiftInventoryFont(10.0f)).Text(FText::FromString(TEXT("Filter")))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleNextPageClicked))
					[
						SNew(STextBlock).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT(">")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUnequipEquipmentSlotClicked, ProjectRiftEquipmentSlots::Armor))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("Unequip Armor")))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUnequipEquipmentSlotClicked, ProjectRiftEquipmentSlots::Chip))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("Unequip Chip")))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUnequipEquipmentSlotClicked, ProjectRiftEquipmentSlots::Tool))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(11.0f)).Text(FText::FromString(TEXT("Unequip Tool")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(ShipResourceSummaryTextBlock, STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.46f, 0.95f, 0.88f, 1.0f)))
				.Font(GetProjectRiftInventoryFont(13.0f))
				.Text(GetShipResourceSummaryText())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(PrimaryWeaponSummaryTextBlock, STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.35f, 1.0f)))
				.Font(GetProjectRiftInventoryFont(13.0f))
				.Text(GetPrimaryWeaponSummaryText())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(EquipmentSummaryTextBlock, STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.52f, 0.86f, 0.92f, 1.0f)))
				.Font(GetProjectRiftInventoryFont(12.0f))
				.Text(GetEquipmentSummaryText())
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SAssignNew(ItemScrollBox, SScrollBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(9.0f))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.09f, 0.1f, 0.92f))
				[
					SAssignNew(DetailTextBlock, STextBlock)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.9f, 0.86f, 1.0f)))
					.Font(GetProjectRiftInventoryFont(12.0f))
					.Text(FText::FromString(TEXT("Select an item.")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUseSelectedClicked))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 0.92f, 1.0f)))
						.Font(GetProjectRiftInventoryFont(13.0f))
						.Text(FText::FromString(TEXT("Use Selected")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleDropSelectedClicked))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 0.92f, 1.0f)))
						.Font(GetProjectRiftInventoryFont(13.0f))
						.Text(FText::FromString(TEXT("Drop One")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleEquipSelectedClicked))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Font(GetProjectRiftInventoryFont(13.0f))
						.Text(FText::FromString(TEXT("Equip Selected")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUnequipPrimaryClicked))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Font(GetProjectRiftInventoryFont(13.0f))
						.Text(FText::FromString(TEXT("Unequip Primary")))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleTransferSelectedClicked))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Font(GetProjectRiftInventoryFont(13.0f))
					.Text(FText::FromString(TEXT("Store / Retrieve Selected")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleConfirmDropClicked))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(12.0f)).Text(FText::FromString(TEXT("Confirm Drop")))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton).OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleCancelDropClicked))
					[
						SNew(STextBlock).Justification(ETextJustify::Center).Font(GetProjectRiftInventoryFont(12.0f)).Text(FText::FromString(TEXT("Cancel")))
					]
				]
			]
		];

	RebuildItemList();
	RefreshSelectedItemDetails();
	RefreshShipResourceSummary();
	RefreshPrimaryWeaponSummary();
	RefreshEquipmentSummary();

	return Widget;
}

void UPRInventoryWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ItemScrollBox.Reset();
	DetailTextBlock.Reset();
	ShipResourceSummaryTextBlock.Reset();
	PrimaryWeaponSummaryTextBlock.Reset();
	EquipmentSummaryTextBlock.Reset();
}

void UPRInventoryWidget::NativeDestruct()
{
	BindViewModel(nullptr);
	if (UPRWeaponComponent* Weapon = GetBoundWeaponComponent())
	{
		Weapon->OnWeaponStateChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleWeaponStateChanged);
	}
	if (UPREquipmentComponent* Equipment = GetBoundEquipmentComponent())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleEquipmentChanged);
	}
	if (UPRInventoryComponent* InventoryComponent = BoundInventory.Get())
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = nullptr;
	Super::NativeDestruct();
}

void UPRInventoryWidget::HandleInventoryChanged()
{
	RefreshInventory();
}

void UPRInventoryWidget::HandleWeaponStateChanged()
{
	RefreshPrimaryWeaponSummary();
	RefreshEquipmentSummary();
	RefreshInventory();
}

void UPRInventoryWidget::HandleEquipmentChanged()
{
	RefreshEquipmentSummary();
}

void UPRInventoryWidget::HandleViewModelChanged()
{
	RefreshInventory();
}

FReply UPRInventoryWidget::HandleItemClicked(const int32 ItemIndex)
{
	SelectDisplayedItem(ItemIndex);
	return FReply::Handled();
}

FReply UPRInventoryWidget::HandleUseSelectedClicked()
{
	RequestUseSelectedItem();
	return FReply::Handled();
}

FReply UPRInventoryWidget::HandleDropSelectedClicked() { BeginDropConfirmationSelectedItem(1); return FReply::Handled(); }

FReply UPRInventoryWidget::HandleEquipSelectedClicked()
{
	RequestEquipSelectedItem();
	return FReply::Handled();
}

FReply UPRInventoryWidget::HandleUnequipPrimaryClicked()
{
	RequestUnequipPrimaryWeapon();
	return FReply::Handled();
}

FReply UPRInventoryWidget::HandleUnequipEquipmentSlotClicked(const FName SlotId)
{
	RequestUnequipEquipmentSlot(SlotId);
	return FReply::Handled();
}

FReply UPRInventoryWidget::HandleShowBackpackClicked() { ShowBackpackPresentation(); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleShowWarehouseClicked() { ShowWarehousePresentation(); return FReply::Handled(); }
FReply UPRInventoryWidget::HandlePreviousPageClicked() { ChangePresentationPage(-1); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleNextPageClicked() { ChangePresentationPage(1); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleCycleSortClicked() { CyclePresentationSort(); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleCycleFilterClicked() { CyclePresentationFilter(); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleTransferSelectedClicked() { RequestTransferSelectedItem(1); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleConfirmDropClicked() { ConfirmPendingInventoryAction(true); return FReply::Handled(); }
FReply UPRInventoryWidget::HandleCancelDropClicked() { ConfirmPendingInventoryAction(false); return FReply::Handled(); }

void UPRInventoryWidget::RebuildItemList()
{
	if (!ItemScrollBox.IsValid())
	{
		return;
	}

	ItemScrollBox->ClearChildren();

	if (DisplayedItems.IsEmpty())
	{
		ItemScrollBox->AddSlot()
		.Padding(0.0f, 4.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.74f, 0.7f, 1.0f)))
			.Font(GetProjectRiftInventoryFont(13.0f))
			.Text(FText::FromString(TEXT("Empty")))
		];
		return;
	}

	for (int32 Index = 0; Index < DisplayedItems.Num(); ++Index)
	{
		const FPRItemInstance Item = DisplayedItems[Index];
		const bool bSelected = Index == SelectedItemIndex;

		ItemScrollBox->AddSlot()
		.Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(bSelected ? FLinearColor(0.26f, 0.31f, 0.38f, 1.0f) : FLinearColor(0.11f, 0.13f, 0.15f, 1.0f))
			.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleItemClicked, Index))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(GetItemRarityColor(Item.Rarity)))
				.Font(GetProjectRiftInventoryFont(13.0f))
				.Text(FText::FromString(BuildItemSummary(Item)))
			]
		];
	}
}

void UPRInventoryWidget::RefreshSelectedItemDetails()
{
	if (DetailTextBlock.IsValid())
	{
		DetailTextBlock->SetText(BuildSelectedItemDetails());
	}
}

void UPRInventoryWidget::RefreshPrimaryWeaponSummary()
{
	if (PrimaryWeaponSummaryTextBlock.IsValid())
	{
		PrimaryWeaponSummaryTextBlock->SetText(GetPrimaryWeaponSummaryText());
	}
}

void UPRInventoryWidget::RefreshEquipmentSummary()
{
	if (EquipmentSummaryTextBlock.IsValid())
	{
		EquipmentSummaryTextBlock->SetText(GetEquipmentSummaryText());
	}
}

UPRWeaponComponent* UPRInventoryWidget::GetBoundWeaponComponent() const
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	const APRPlayerState* PlayerState = InventoryComponent ? Cast<APRPlayerState>(InventoryComponent->GetOwner()) : nullptr;
	return PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
}

UPREquipmentComponent* UPRInventoryWidget::GetBoundEquipmentComponent() const
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	const APRPlayerState* PlayerState = InventoryComponent ? Cast<APRPlayerState>(InventoryComponent->GetOwner()) : nullptr;
	return PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
}

void UPRInventoryWidget::RefreshShipResourceSummary()
{
	if (ShipResourceSummaryTextBlock.IsValid())
	{
		ShipResourceSummaryTextBlock->SetText(GetShipResourceSummaryText());
	}
}

UPRItemDataAsset* UPRInventoryWidget::FindItemData(const FPRItemInstance& Item) const
{
	const UPRInventoryComponent* InventoryComponent = BoundInventory.Get();
	return InventoryComponent ? InventoryComponent->FindItemData(Item.ItemId) : nullptr;
}

FString UPRInventoryWidget::BuildItemSummary(const FPRItemInstance& Item) const
{
	return FString::Printf(
		TEXT("%s    x%d    %s    Lv.%d"),
		*GetItemDisplayName(Item).ToString(),
		Item.Count,
		*GetItemRarityText(Item.Rarity).ToString(),
		Item.Level);
}

FText UPRInventoryWidget::BuildSelectedItemDetails() const
{
	if (PendingConfirmation.IsValid())
	{
		return FText::FromString(PendingConfirmation.Prompt.ToString() + LINE_TERMINATOR + TEXT("Confirm or Cancel to continue."));
	}
	if (!HasSelectedItem())
	{
		return DisplayedItems.IsEmpty()
			? FText::FromString(TEXT("Inventory is empty."))
			: FText::FromString(TEXT("Select an item."));
	}

	return GetItemTooltipText(DisplayedItems[SelectedItemIndex]);
}

bool UPRInventoryWidget::IsPresentingWarehouse() const
{
	const UPRInventoryViewModel* ViewModel = BoundViewModel.Get();
	return ViewModel && ViewModel->GetSource() == EPRInventoryPresentationSource::Warehouse;
}
