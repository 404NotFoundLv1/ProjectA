#include "UI/PRCraftingWidget.h"

#include "Core/PRAssetManager.h"
#include "Crafting/PRCraftingRecipeDataAsset.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "InputCoreTypes.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PRInventoryComponent.h"
#include "Misc/Paths.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FSlateFontInfo CraftingFont(const float Size)
{
	static const TSharedPtr<const FCompositeFont> Font = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	return FSlateFontInfo(Font, Size);
}
}

UPRCraftingWidget::UPRCraftingWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UPRCraftingWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Recipes = SNew(SVerticalBox);
	TArray<UPRCraftingRecipeDataAsset*> Catalog;
	if (UPRAssetManager* Assets = UPRAssetManager::Get())
	{
		Assets->LoadCraftingRecipeCatalog(Catalog);
	}
	for (const UPRCraftingRecipeDataAsset* Recipe : Catalog)
	{
		if (!Recipe) { continue; }
		const FName RecipeId = Recipe->RecipeId;
		Recipes->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, RecipeId]() { return HandleCraftClicked(RecipeId); })
			[
				SNew(STextBlock).Font(CraftingFont(14.0f)).Text(FText::FromString(FString::Printf(TEXT("Craft: %s"), *RecipeId.ToString())))
			]
		];
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Font(CraftingFont(24.0f)).Text(FText::FromString(TEXT("CRAFTING TERMINAL")))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked_UObject(this, &UPRCraftingWidget::HandleCloseClicked)[SNew(STextBlock).Text(FText::FromString(TEXT("Close [Esc]")))]]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 10.0f)
		[
			SNew(SBorder).Padding(12.0f).BorderBackgroundColor(FLinearColor(0.035f, 0.05f, 0.065f, 0.96f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[SAssignNew(SummaryTextBlock, STextBlock).AutoWrapText(true).Font(CraftingFont(15.0f))]
				+ SScrollBox::Slot().Padding(0.0f, 10.0f)[Recipes]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[SAssignNew(StatusTextBlock, STextBlock).AutoWrapText(true).Font(CraftingFont(14.0f)).ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.35f, 1.0f)))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)[SNew(SButton).OnClicked_UObject(this, &UPRCraftingWidget::HandleNextItemClicked)[SNew(STextBlock).Text(FText::FromString(TEXT("Select Equipment")))]]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f)[SNew(SButton).OnClicked_UObject(this, &UPRCraftingWidget::HandleUpgradeClicked)[SNew(STextBlock).Text(FText::FromString(TEXT("Upgrade")))]]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)[SNew(SButton).OnClicked_UObject(this, &UPRCraftingWidget::HandleDismantleClicked)[SNew(STextBlock).Text(FText::FromString(TEXT("Dismantle")))]]
		];
	RefreshCraftingDisplay();
	return SNew(SBorder).Padding(18.0f).BorderBackgroundColor(FLinearColor(0.008f, 0.014f, 0.022f, 0.98f))[SNew(SBox).MinDesiredWidth(680.0f).MinDesiredHeight(460.0f)[Content]];
}

FReply UPRCraftingWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape) { HandleCloseClicked(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UPRCraftingWidget::CraftRecipe(const FName RecipeId) { if (APRPlayerController* C = GetOwningPlayer<APRPlayerController>()) { C->RequestCraftRecipe(RecipeId); LocalStatus = TEXT("Craft request sent."); RefreshCraftingDisplay(); } }
void UPRCraftingWidget::DismantleItem(const FGuid InstanceGuid) { if (APRPlayerController* C = GetOwningPlayer<APRPlayerController>()) { C->RequestDismantle(InstanceGuid); } }
void UPRCraftingWidget::UpgradeItem(const FGuid InstanceGuid) { if (APRPlayerController* C = GetOwningPlayer<APRPlayerController>()) { C->RequestUpgrade(InstanceGuid); } }
FString UPRCraftingWidget::GetStatusText() const { const APRPlayerController* C = GetOwningPlayer<APRPlayerController>(); return C ? C->GetCraftingSaveStatus() : TEXT("No player controller"); }

FReply UPRCraftingWidget::HandleCraftClicked(const FName RecipeId) { CraftRecipe(RecipeId); return FReply::Handled(); }
FReply UPRCraftingWidget::HandleNextItemClicked() { SelectNextUpgradeableItem(); RefreshCraftingDisplay(); return FReply::Handled(); }
FReply UPRCraftingWidget::HandleDismantleClicked() { if (SelectedItemGuid.IsValid()) { DismantleItem(SelectedItemGuid); LocalStatus = TEXT("Dismantle request sent."); } else { LocalStatus = TEXT("Select a backpack equipment instance first."); } RefreshCraftingDisplay(); return FReply::Handled(); }
FReply UPRCraftingWidget::HandleUpgradeClicked() { if (SelectedItemGuid.IsValid()) { UpgradeItem(SelectedItemGuid); LocalStatus = TEXT("Upgrade request sent."); } else { LocalStatus = TEXT("Select an equipment instance first."); } RefreshCraftingDisplay(); return FReply::Handled(); }
FReply UPRCraftingWidget::HandleCloseClicked() { if (APRPlayerController* C = GetOwningPlayer<APRPlayerController>()) { C->HideCraftingPanel(); } return FReply::Handled(); }

void UPRCraftingWidget::SelectNextUpgradeableItem()
{
	APRPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APRPlayerState>() : nullptr;
	UPRWeaponComponent* Weapon = State ? State->GetWeaponComponent() : nullptr;
	if (!State || !Weapon) { SelectedItemGuid.Invalidate(); return; }
	TArray<FPRItemInstance> Candidates;
	for (const FPRItemInstance& Item : State->GetInventoryComponent()->GetInventoryItems())
	{
		if (UPRAssetManager* Assets = UPRAssetManager::Get(); Cast<UPREquipmentDataAsset>(Assets ? Assets->LoadItemDataSync(Item.ItemId) : nullptr)) { Candidates.Add(Item); }
	}
	for (const FPRProfileEquipmentEntry& Entry : Weapon->GetEquipmentEntries()) { Candidates.Add(Entry.Item); }
	int32 Current = Candidates.IndexOfByPredicate([this](const FPRItemInstance& Item) { return Item.InstanceGuid == SelectedItemGuid; });
	SelectedItemGuid = Candidates.IsValidIndex(Current + 1) ? Candidates[Current + 1].InstanceGuid : (Candidates.IsEmpty() ? FGuid() : Candidates[0].InstanceGuid);
}

FText UPRCraftingWidget::BuildSummary() const
{
	APRPlayerState* State = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APRPlayerState>() : nullptr;
	if (!State) { return FText::FromString(TEXT("Waiting for player profile.")); }
	FString Selected = TEXT("None");
	if (SelectedItemGuid.IsValid()) { Selected = SelectedItemGuid.ToString(EGuidFormats::Digits); }
	return FText::FromString(FString::Printf(TEXT("Resources: %s\nSelected equipment: %s\nChoose a recipe below, or select equipment to upgrade/dismantle."), *State->BuildShipResourceSummary(), *Selected));
}

FText UPRCraftingWidget::BuildStatus() const { return FText::FromString(LocalStatus.IsEmpty() ? GetStatusText() : LocalStatus + TEXT("\n") + GetStatusText()); }
void UPRCraftingWidget::RefreshCraftingDisplay() { if (SummaryTextBlock) { SummaryTextBlock->SetText(BuildSummary()); } if (StatusTextBlock) { StatusTextBlock->SetText(BuildStatus()); } }
