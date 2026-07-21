#include "UI/PREnemyHealthBarWidget.h"

#include "Enemies/PREnemyCharacter.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void UPREnemyHealthBarWidget::SetObservedEnemy(APREnemyCharacter* InObservedEnemy)
{
	ObservedEnemy = InObservedEnemy;
	RefreshSlate();
}

TSharedRef<SWidget> UPREnemyHealthBarWidget::RebuildWidget()
{
	TSharedRef<SWidget> Widget = SNew(SBorder)
		.Padding(FMargin(6.0f, 4.0f))
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.02f, 0.0f, 0.0f, 0.68f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SAssignNew(HealthTextBlock, STextBlock)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.22f, 0.16f, 1.0f)))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
				.ShadowColorAndOpacity(FLinearColor::Black)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.Text(BuildHealthText())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SAssignNew(HealthProgressBar, SProgressBar)
				.Percent(GetObservedHealthPercent())
				.FillColorAndOpacity(FLinearColor(0.92f, 0.05f, 0.03f, 1.0f))
			]
		];

	RefreshSlate();
	return Widget;
}

void UPREnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshSlate();
}

void UPREnemyHealthBarWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	HealthProgressBar.Reset();
	HealthTextBlock.Reset();
}

float UPREnemyHealthBarWidget::GetObservedHealthPercent() const
{
	const APREnemyCharacter* Enemy = ObservedEnemy.Get();
	return Enemy ? Enemy->GetHealthPercent() : 1.0f;
}

FText UPREnemyHealthBarWidget::BuildHealthText() const
{
	const APREnemyCharacter* Enemy = ObservedEnemy.Get();
	if (!Enemy)
	{
		return FText::FromString(TEXT("HOSTILE"));
	}

	const FString StatusText = Enemy->GetActiveStatusText();
	const UPREnemyDefinitionDataAsset* Definition = Enemy->GetEnemyDefinition();
	const FString EnemyName = Definition && !Definition->DisplayName.IsEmpty()
		? Definition->DisplayName.ToString() : TEXT("HOSTILE");
	return FText::FromString(StatusText == TEXT("None")
		? FString::Printf(TEXT("%s %.0f / %.0f"), *EnemyName, Enemy->GetHealth(), Enemy->GetMaxHealth())
		: FString::Printf(TEXT("%s %.0f / %.0f  [%s]"), *EnemyName, Enemy->GetHealth(), Enemy->GetMaxHealth(), *StatusText));
}

void UPREnemyHealthBarWidget::RefreshSlate()
{
	if (HealthProgressBar.IsValid())
	{
		HealthProgressBar->SetPercent(GetObservedHealthPercent());
	}

	if (HealthTextBlock.IsValid())
	{
		HealthTextBlock->SetText(BuildHealthText());
	}
}
