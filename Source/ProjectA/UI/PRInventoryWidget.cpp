#include "UI/PRInventoryWidget.h"

#include "Fonts/SlateFontInfo.h"
#include "Items/PRInventoryComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void UPRInventoryWidget::BindInventory(UPRInventoryComponent* InInventoryComponent)
{
	if (UPRInventoryComponent* CurrentInventory = BoundInventory.Get())
	{
		CurrentInventory->OnInventoryChanged.RemoveDynamic(this, &UPRInventoryWidget::HandleInventoryChanged);
	}

	BoundInventory = InInventoryComponent;
	SelectedItemIndex = INDEX_NONE;

	if (InInventoryComponent)
	{
		InInventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &UPRInventoryWidget::HandleInventoryChanged);
	}

	RefreshInventory();
}

void UPRInventoryWidget::RefreshInventory()
{
	DisplayedItems.Reset();

	if (UPRInventoryComponent* InventoryComponent = BoundInventory.Get())
	{
		DisplayedItems = InventoryComponent->GetInventoryItems();
	}

	if (!DisplayedItems.IsValidIndex(SelectedItemIndex))
	{
		SelectedItemIndex = INDEX_NONE;
	}

	RebuildItemList();
	RefreshSelectedItemDetails();
}

void UPRInventoryWidget::SelectDisplayedItem(const int32 ItemIndex)
{
	SelectedItemIndex = DisplayedItems.IsValidIndex(ItemIndex) ? ItemIndex : INDEX_NONE;
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
	if (Item.ItemId.IsNone())
	{
		return FText::FromString(TEXT("Unknown Item"));
	}

	return FText::FromString(FName::NameToDisplayString(Item.ItemId.ToString(), false));
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

	if (!Item.Affixes.IsEmpty())
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

void UPRInventoryWidget::RequestUseSelectedItem()
{
	if (HasSelectedItem())
	{
		OnUseItemRequested.Broadcast(DisplayedItems[SelectedItemIndex].ItemId);
	}
}

void UPRInventoryWidget::RequestUseDisplayedItem(const int32 ItemIndex)
{
	if (DisplayedItems.IsValidIndex(ItemIndex))
	{
		SelectedItemIndex = ItemIndex;
		OnUseItemRequested.Broadcast(DisplayedItems[ItemIndex].ItemId);
		RebuildItemList();
		RefreshSelectedItemDetails();
	}
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
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
				.Text(FText::FromString(TEXT("Inventory")))
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
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12))
					.Text(FText::FromString(TEXT("Select an item.")))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.OnClicked(FOnClicked::CreateUObject(this, &UPRInventoryWidget::HandleUseSelectedClicked))
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 0.92f, 1.0f)))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
					.Text(FText::FromString(TEXT("Use Selected")))
				]
			]
		];

	RebuildItemList();
	RefreshSelectedItemDetails();

	return Widget;
}

void UPRInventoryWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ItemScrollBox.Reset();
	DetailTextBlock.Reset();
}

void UPRInventoryWidget::NativeDestruct()
{
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
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
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
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
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
	if (!HasSelectedItem())
	{
		return DisplayedItems.IsEmpty()
			? FText::FromString(TEXT("Inventory is empty."))
			: FText::FromString(TEXT("Select an item."));
	}

	return GetItemTooltipText(DisplayedItems[SelectedItemIndex]);
}
