#include "UI/PRRiftSettlementWidget.h"

#include "Core/PRRiftGameState.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void UPRRiftSettlementWidget::SetPreviewSettlementData(const FPRRiftSettlementData& InSettlementData)
{
	bUsePreviewData = true;
	DisplayedSettlementData = InSettlementData;
	RefreshSettlementSlate();
}

void UPRRiftSettlementWidget::ClearPreviewSettlementData()
{
	bUsePreviewData = false;
	DisplayedSettlementData = FPRRiftSettlementData();
	RefreshSettlementSlate();
}

FText UPRRiftSettlementWidget::GetDisplayedSettlementText() const
{
	return BuildSettlementText(DisplayedSettlementData);
}

TSharedRef<SWidget> UPRRiftSettlementWidget::RebuildWidget()
{
	TSharedRef<SWidget> Widget = SAssignNew(SettlementRootWidget, SBorder)
		.Padding(FMargin(26.0f, 22.0f))
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.86f))
		[
			SAssignNew(SettlementTextBlock, STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.84f, 1.0f, 1.0f, 1.0f)))
			.Font(GetSettlementFont(24.0f))
			.Justification(ETextJustify::Left)
			.LineHeightPercentage(1.15f)
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.Text(GetDisplayedSettlementText())
		];

	RefreshSettlementSlate();
	return Widget;
}

void UPRRiftSettlementWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SettlementRootWidget.Reset();
	SettlementTextBlock.Reset();
}

void UPRRiftSettlementWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bUsePreviewData)
	{
		RefreshSettlementFromGameState();
	}
}

void UPRRiftSettlementWidget::RefreshSettlementFromGameState()
{
	const UWorld* World = GetWorld();
	const APRRiftGameState* RiftGameState = World ? World->GetGameState<APRRiftGameState>() : nullptr;
	if (RiftGameState && RiftGameState->IsSettlementReady())
	{
		DisplayedSettlementData = RiftGameState->GetSettlementData();
	}
	else
	{
		DisplayedSettlementData = FPRRiftSettlementData();
	}

	RefreshSettlementSlate();
}

void UPRRiftSettlementWidget::RefreshSettlementSlate()
{
	const bool bShouldDisplay = DisplayedSettlementData.IsValid();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (SettlementRootWidget.IsValid())
	{
		SettlementRootWidget->SetVisibility(bShouldDisplay ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	if (!SettlementTextBlock.IsValid())
	{
		return;
	}

	SettlementTextBlock->SetText(GetDisplayedSettlementText());
	SettlementTextBlock->SetFont(GetSettlementFont(24.0f));
}

FText UPRRiftSettlementWidget::BuildSettlementText(const FPRRiftSettlementData& InSettlementData) const
{
	if (!InSettlementData.IsValid())
	{
		return FText::GetEmpty();
	}

	const TCHAR* ResultText = TEXT("\u672A\u77E5");
	if (InSettlementData.Result == EPRRiftMissionResult::Success)
	{
		ResultText = TEXT("\u64A4\u79BB\u6210\u529F");
	}
	else if (InSettlementData.Result == EPRRiftMissionResult::Failed)
	{
		ResultText = TEXT("\u4EFB\u52A1\u5931\u8D25");
	}

	return FText::FromString(FString::Printf(
		TEXT("\u526F\u672C\u7ED3\u7B97\n\n")
		TEXT("\u7ED3\u679C\uFF1A%s\n")
		TEXT("\u4EFB\u52A1\u65F6\u95F4\uFF1A%.1f s\n")
		TEXT("\u64A4\u79BB\u4EBA\u6570\uFF1A%d / %d\n")
		TEXT("\u4EFB\u52A1\u8FDB\u5EA6\uFF1A%.0f%%\n")
		TEXT("\u88C2\u9699\u7A33\u5B9A\u5EA6\uFF1A%.0f%%\n")
		TEXT("\u5E26\u51FA\u8D44\u6E90\uFF1A%d \u4EF6 / %d \u7C7B\n")
		TEXT("\u635F\u5931\u8D44\u6E90\uFF1A%d \u4EF6\n")
		TEXT("\u5E26\u51FA\u7269\u54C1\uFF1A%d \u4EF6 / %d \u7C7B\n\n")
		TEXT("\u5373\u5C06\u8FD4\u56DE\u4E3B\u623F\u95F4..."),
		ResultText,
		InSettlementData.MissionTime,
		InSettlementData.ExtractedPlayerCount,
		InSettlementData.AlivePlayerCount,
		InSettlementData.ObjectiveProgress * 100.0f,
		InSettlementData.RiftStability,
		InSettlementData.ExtractedResourceCount,
		InSettlementData.ExtractedUniqueResourceTypes,
		InSettlementData.LostResourceCount,
		InSettlementData.ExtractedItemCount,
		InSettlementData.ExtractedUniqueItemTypes));
}

FSlateFontInfo UPRRiftSettlementWidget::GetSettlementFont(const float Size)
{
	static const TSharedPtr<const FCompositeFont> SettlementFont = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"),
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
		EFontHinting::Default,
		EFontLoadingPolicy::LazyLoad);

	return FSlateFontInfo(SettlementFont, Size);
}
