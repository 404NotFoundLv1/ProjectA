#include "UI/PRBossHUDWidget.h"

#include "Core/PRRiftGameState.h"
#include "GameFramework/PlayerController.h"
#include "SlateBasics.h"
#include "Widgets/Layout/SBox.h"

namespace
{
const TCHAR* ToBossStateText(const EPRBossRuntimeState State)
{
	switch (State)
	{
	case EPRBossRuntimeState::Telegraphing: return TEXT("TELEGRAPH");
	case EPRBossRuntimeState::Executing: return TEXT("EXECUTING");
	case EPRBossRuntimeState::PhaseTransition: return TEXT("PHASE TRANSITION");
	case EPRBossRuntimeState::Staggered: return TEXT("STAGGERED");
	case EPRBossRuntimeState::Suspended: return TEXT("SUSPENDED");
	case EPRBossRuntimeState::Defeated: return TEXT("DEFEATED");
	default: return TEXT("ACTIVE");
	}
}
}

void UPRBossHUDWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

FText UPRBossHUDWidget::GetBossText() const
{
	const APRRiftGameState* RiftGameState = LocalController.IsValid() ? LocalController->GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
	const FPRBossRuntimeSnapshot Snapshot = RiftGameState ? RiftGameState->GetBossRuntimeSnapshot() : FPRBossRuntimeSnapshot();
	if (!Snapshot.bVisible)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(
		TEXT("BOSS %s  %.0f%%  [%s]\nPhase: %s | Pattern: %s | Target: %s%s%s"),
		*Snapshot.BossId.ToString(),
		Snapshot.HealthPercent * 100.0f,
		ToBossStateText(Snapshot.State),
		*Snapshot.PhaseId.ToString(),
		*Snapshot.ActivePatternId.ToString(),
		*Snapshot.PrimaryTargetId.ToString(),
		Snapshot.bInterruptible ? TEXT(" | INTERRUPTIBLE") : TEXT(""),
		Snapshot.bStaggered ? TEXT(" | STAGGERED") : TEXT("")));
}

TSharedRef<SWidget> UPRBossHUDWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);
	Panel->AddSlot().AutoHeight().Padding(8.0f)
	[
		SAssignNew(BossText, STextBlock)
		.Text(GetBossText())
		.ColorAndOpacity(FLinearColor(1.0f, 0.32f, 0.08f, 1.0f))
	];
	Panel->AddSlot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
	[
		SNew(SBox).WidthOverride(420.0f)
		[
			SAssignNew(HealthBar, SProgressBar)
			.Percent(1.0f)
			.FillColorAndOpacity(FLinearColor(0.95f, 0.16f, 0.05f, 1.0f))
		]
	];
	return Panel;
}

void UPRBossHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshText();
}

void UPRBossHUDWidget::RefreshText()
{
	if (BossText.IsValid())
	{
		BossText->SetText(GetBossText());
	}
	if (HealthBar.IsValid())
	{
		const APRRiftGameState* RiftGameState = LocalController.IsValid() ? LocalController->GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
		const FPRBossRuntimeSnapshot Snapshot = RiftGameState ? RiftGameState->GetBossRuntimeSnapshot() : FPRBossRuntimeSnapshot();
		HealthBar->SetPercent(Snapshot.bVisible ? FMath::Clamp(Snapshot.HealthPercent, 0.0f, 1.0f) : 0.0f);
	}
}
