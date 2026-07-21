#include "UI/PRObjectiveTrackerWidget.h"

#include "Core/PRRiftGameState.h"
#include "GameFramework/PlayerController.h"
#include "SlateBasics.h"

namespace
{
const TCHAR* RiskTierToText(const EPRRiftRiskTier Tier)
{
	switch (Tier)
	{
	case EPRRiftRiskTier::Unstable: return TEXT("UNSTABLE");
	case EPRRiftRiskTier::Critical: return TEXT("CRITICAL");
	case EPRRiftRiskTier::Collapse: return TEXT("COLLAPSE");
	default: return TEXT("STABLE");
	}
}
}

void UPRObjectiveTrackerWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

FText UPRObjectiveTrackerWidget::GetObjectiveTrackerText() const
{
	const APRRiftGameState* RiftGameState = LocalController.IsValid() ? LocalController->GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
	if (!RiftGameState)
	{
		return FText::GetEmpty();
	}
	const FPRRiftRiskSnapshot Risk = RiftGameState->GetRiftRiskSnapshot();
	FString Text = FString::Printf(TEXT("STABILITY %.0f%% | %s\nOBJECTIVES\n"), Risk.Stability, RiskTierToText(Risk.CurrentTier));
	for (const FPRObjectiveSummary& Summary : RiftGameState->GetObjectiveSummaries())
	{
		const TCHAR* State = Summary.State == EPRObjectiveNodeState::Completed ? TEXT("✓")
			: Summary.State == EPRObjectiveNodeState::Active ? TEXT("•") : TEXT("○");
		const FString TimerText = Summary.bOptional && Summary.RemainingTimeSeconds > 0.0f
			? FString::Printf(TEXT(" %.0fs"), Summary.RemainingTimeSeconds) : FString();
		Text += FString::Printf(TEXT("%s %s%s %d/%d%s\n"), State, Summary.bOptional ? TEXT("[Optional] ") : TEXT(""),
			*Summary.DisplayName.ToString(), Summary.CurrentCount, Summary.TargetCount, *TimerText);
	}
	for (const FPRMissionModifierSummary& Modifier : RiftGameState->GetMissionModifierSummaries())
	{
		Text += FString::Printf(TEXT("MOD: %s\n"), *Modifier.DisplayName.ToString());
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
