#include "UI/PRLobbyReadyDebugWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UPRLobbyReadyDebugWidget::RebuildWidget()
{
	const FText InitialText = PendingText.IsEmpty()
		? FText::FromString(TEXT("Lobby Ready\nWaiting for game state..."))
		: PendingText;

	return SNew(SBorder)
		.Padding(FMargin(12.0f))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f))
		[
			SAssignNew(ReadyTextBlock, STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor::Green))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
			.Text(InitialText)
		];
}

void UPRLobbyReadyDebugWidget::SetReadyText(const FText& InText)
{
	PendingText = InText;
	if (ReadyTextBlock.IsValid())
	{
		ReadyTextBlock->SetText(PendingText);
	}
}
