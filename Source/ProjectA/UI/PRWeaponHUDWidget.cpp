#include "UI/PRWeaponHUDWidget.h"

#include "Items/PRWeaponDataAsset.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void UPRWeaponHUDWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

TSharedRef<SWidget> UPRWeaponHUDWidget::RebuildWidget()
{
	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(CrosshairText, STextBlock)
			.Text(FText::FromString(TEXT("+")))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))
			.ColorAndOpacity(FLinearColor(0.75f, 0.95f, 1.0f, 0.95f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 42.0f, 44.0f))
		[
			SAssignNew(AmmoText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
			.ColorAndOpacity(FLinearColor::White)
		];
}

void UPRWeaponHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshText();
}

void UPRWeaponHUDWidget::RefreshText()
{
	const APlayerController* Controller = LocalController.Get();
	const APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	const UPRWeaponComponent* Weapon = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = Weapon ? Weapon->GetEquippedWeaponData() : nullptr;
	if (CrosshairText)
	{
		CrosshairText->SetVisibility(WeaponData ? EVisibility::Visible : EVisibility::Collapsed);
		CrosshairText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Weapon && Weapon->IsAiming() ? 16 : 22));
	}
	if (AmmoText)
	{
		if (!WeaponData)
		{
			AmmoText->SetText(FText::GetEmpty());
			return;
		}
		FString Status = Weapon->IsReloading()
			? TEXT("RELOADING")
			: Weapon->GetMagazineAmmo() <= 0 ? TEXT("EMPTY / PRESS R") : TEXT("");
		AmmoText->SetText(FText::FromString(FString::Printf(
			TEXT("%s\n%d / %d%s%s"),
			*WeaponData->DisplayName.ToString(),
			Weapon->GetMagazineAmmo(),
			Weapon->GetReserveAmmo(),
			Status.IsEmpty() ? TEXT("") : TEXT("\n"),
			*Status)));
	}
}
