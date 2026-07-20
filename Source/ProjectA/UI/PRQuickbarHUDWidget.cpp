#include "UI/PRQuickbarHUDWidget.h"

#include "Items/PRInventoryComponent.h"
#include "Items/PRQuickbarComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "SlateBasics.h"

void UPRQuickbarHUDWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

FText UPRQuickbarHUDWidget::GetQuickbarText() const
{
	const APRPlayerState* PlayerState = LocalController.IsValid() ? LocalController->GetPlayerState<APRPlayerState>() : nullptr;
	const UPRQuickbarComponent* Quickbar = PlayerState ? PlayerState->GetQuickbarComponent() : nullptr;
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!Quickbar || !Inventory)
	{
		return FText::GetEmpty();
	}
	const TArray<FPRQuickSlotReference> Slots = Quickbar->GetQuickSlots();
	FString Text;
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		const FPRQuickSlotReference* SlotReference = Slots.FindByPredicate([SlotIndex](const FPRQuickSlotReference& Entry) { return Entry.SlotIndex == SlotIndex; });
		const FPRItemInstance* Item = SlotReference ? Inventory->GetItems().FindByPredicate([SlotReference](const FPRItemInstance& Candidate) { return Candidate.InstanceGuid == SlotReference->InstanceGuid; }) : nullptr;
		Text += FString::Printf(TEXT("[%d %s] "), SlotIndex + 1, Item ? *FString::Printf(TEXT("%s x%d"), *Item->ItemId.ToString(), Item->Count) : TEXT("Empty"));
	}
	const FPRQuickbarUseState ActiveUse = Quickbar->GetActiveUse();
	if (ActiveUse.IsActive())
	{
		Text += TEXT(" USING");
	}
	return FText::FromString(Text);
}

TSharedRef<SWidget> UPRQuickbarHUDWidget::RebuildWidget()
{
	SAssignNew(QuickbarText, STextBlock)
		.Text(GetQuickbarText())
		.ColorAndOpacity(FLinearColor(0.7f, 0.9f, 1.0f, 1.0f));
	return QuickbarText.ToSharedRef();
}

void UPRQuickbarHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshText();
}

void UPRQuickbarHUDWidget::RefreshText()
{
	if (QuickbarText.IsValid())
	{
		QuickbarText->SetText(GetQuickbarText());
	}
}
