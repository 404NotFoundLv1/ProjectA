#include "UI/PRInteractionPromptWidget.h"

#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void UPRInteractionPromptWidget::SetPromptText(const FText& InPromptText)
{
	PromptText = InPromptText;
	RefreshPromptSlate();
}

void UPRInteractionPromptWidget::SetPromptColor(const FLinearColor& InPromptColor)
{
	PromptColor = InPromptColor;
	RefreshPromptSlate();
}

TSharedRef<SWidget> UPRInteractionPromptWidget::RebuildWidget()
{
	TSharedRef<SWidget> Widget = SNew(SBorder)
		.Padding(FMargin(10.0f, 4.0f))
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f))
		[
			SAssignNew(PromptTextBlock, STextBlock)
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(FSlateColor(PromptColor))
			.Font(GetPromptFont())
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.Text(PromptText)
		];

	RefreshPromptSlate();
	return Widget;
}

void UPRInteractionPromptWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PromptTextBlock.Reset();
}

FSlateFontInfo UPRInteractionPromptWidget::GetPromptFont() const
{
	static const TSharedPtr<const FCompositeFont> PromptFont = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"),
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
		EFontHinting::Default,
		EFontLoadingPolicy::LazyLoad);

	return FSlateFontInfo(PromptFont, 26.0f);
}

void UPRInteractionPromptWidget::RefreshPromptSlate()
{
	if (!PromptTextBlock.IsValid())
	{
		return;
	}

	PromptTextBlock->SetText(PromptText);
	PromptTextBlock->SetColorAndOpacity(FSlateColor(PromptColor));
	PromptTextBlock->SetFont(GetPromptFont());
}
