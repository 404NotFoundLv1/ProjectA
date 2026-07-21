#include "UI/PRObjectiveTrackerWidget.h"

#include "Core/PRRiftGameState.h"
#include "GameFramework/PlayerController.h"
#include "SlateBasics.h"

void UPRObjectiveTrackerWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

FText UPRObjectiveTrackerWidget::GetObjectiveTrackerText() const
{
	const APRRiftGameState* RiftGameState = LocalController.IsValid() ? LocalController->GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
	if (!RiftGameState || RiftGameState->GetObjectiveSummaries().IsEmpty())
	{
		return FText::GetEmpty();
	}
	FString Text(TEXT("OBJECTIVES\n"));
	for (const FPRObjectiveSummary& Summary : RiftGameState->GetObjectiveSummaries())
	{
		const TCHAR* State = Summary.State == EPRObjectiveNodeState::Completed ? TEXT("✓")
			: Summary.State == EPRObjectiveNodeState::Active ? TEXT("•") : TEXT("○");
		Text += FString::Printf(TEXT("%s %s%s %d/%d\n"), State, Summary.bOptional ? TEXT("[Optional] ") : TEXT(""),
			*Summary.DisplayName.ToString(), Summary.CurrentCount, Summary.TargetCount);
	}
	return FText::FromString(Text);
}

TSharedRef<SWidget> UPRObjectiveTrackerWidget::RebuildWidget()
{
	SAssignNew(TrackerText, STextBlock)
		.Text(GetObjectiveTrackerText())
		.ColorAndOpacity(FLinearColor(0.78f, 0.92f, 1.0f, 1.0f));
	return TrackerText.ToSharedRef();
}

void UPRObjectiveTrackerWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshText();
}

void UPRObjectiveTrackerWidget::RefreshText()
{
	if (TrackerText.IsValid())
	{
		TrackerText->SetText(GetObjectiveTrackerText());
	}
}
