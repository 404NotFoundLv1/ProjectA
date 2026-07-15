#include "UI/PRWeaponHUDWidget.h"

#include "Items/PRWeaponDataAsset.h"
#include "Player/PRPlayerState.h"
#include "Weapons/PRWeaponComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr float HitMarkerLifetimeSeconds = 0.15f;
constexpr float DamageNumberLifetimeSeconds = 0.65f;
constexpr float LocalHitPulseLifetimeSeconds = 0.22f;

const FLinearColor ShieldColor(0.05f, 0.75f, 1.0f, 1.0f);
const FLinearColor ShieldBreakColor(0.25f, 1.0f, 1.0f, 1.0f);

bool IsValidDamageAmount(const float Amount)
{
	return FMath::IsFinite(Amount) && Amount > 0.0f;
}

FText FormatDamageAmount(const float Amount)
{
	return FText::FromString(FMath::IsNearlyEqual(Amount, FMath::RoundToFloat(Amount), KINDA_SMALL_NUMBER)
		? FString::Printf(TEXT("%.0f"), Amount)
		: FString::Printf(TEXT("%.1f"), Amount));
}
}

void UPRWeaponHUDWidget::InitializeForController(APlayerController* InController)
{
	LocalController = InController;
	RefreshText();
}

void UPRWeaponHUDWidget::PushHitConfirmation(const FPRHitConfirmation& Confirmation)
{
	const bool bHasShieldDamage = IsValidDamageAmount(Confirmation.ShieldDamage);
	const bool bHasHealthDamage = IsValidDamageAmount(Confirmation.HealthDamage);
	if (!bHasShieldDamage && !bHasHealthDamage)
	{
		return;
	}

	HitMarkerRemainingSeconds = HitMarkerLifetimeSeconds;
	HitMarkerColor = Confirmation.bLethal && bHasHealthDamage
		? FLinearColor::Red
		: Confirmation.bShieldBroken && bHasShieldDamage
			? ShieldBreakColor
			: bHasHealthDamage
				? FLinearColor::White
				: ShieldColor;

	if (bHasShieldDamage)
	{
		FPRHUDDamageNumber& Number = ActiveDamageNumbers.AddDefaulted_GetRef();
		Number.Amount = Confirmation.ShieldDamage;
		Number.Color = Confirmation.bShieldBroken ? ShieldBreakColor : ShieldColor;
		Number.RemainingSeconds = DamageNumberLifetimeSeconds;
		Number.DisplayOffset = ActiveDamageNumbers.Num() > 1 ? 10.0f : 0.0f;
	}
	if (bHasHealthDamage)
	{
		FPRHUDDamageNumber& Number = ActiveDamageNumbers.AddDefaulted_GetRef();
		Number.Amount = Confirmation.HealthDamage;
		Number.Color = Confirmation.bLethal ? FLinearColor::Red : FLinearColor::White;
		Number.RemainingSeconds = DamageNumberLifetimeSeconds;
		Number.DisplayOffset = ActiveDamageNumbers.Num() > 1 ? 24.0f : 0.0f;
	}
	RefreshHitFeedbackSlate();
}

void UPRWeaponHUDWidget::PushLocalHitFeedback(const FPRResolvedDamage& ResolvedDamage)
{
	LocalHitDirection = ResolvedDamage.IncomingDirection.ContainsNaN()
		? FVector::ZeroVector
		: ResolvedDamage.IncomingDirection;
	LocalHitPulseRemaining = LocalHitPulseLifetimeSeconds;
	RefreshHitFeedbackSlate();
}

void UPRWeaponHUDWidget::AdvanceHitFeedback(const float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return;
	}

	HitMarkerRemainingSeconds = FMath::Max(0.0f, HitMarkerRemainingSeconds - DeltaSeconds);
	LocalHitPulseRemaining = FMath::Max(0.0f, LocalHitPulseRemaining - DeltaSeconds);
	for (FPRHUDDamageNumber& Number : ActiveDamageNumbers)
	{
		Number.RemainingSeconds = FMath::Max(0.0f, Number.RemainingSeconds - DeltaSeconds);
		Number.DisplayOffset += DeltaSeconds * 34.0f;
	}
	ActiveDamageNumbers.RemoveAll([](const FPRHUDDamageNumber& Number)
	{
		return !FMath::IsFinite(Number.RemainingSeconds) || Number.RemainingSeconds <= 0.0f;
	});

	if (LocalHitPulseRemaining <= 0.0f)
	{
		LocalHitDirection = FVector::ZeroVector;
	}
	RefreshHitFeedbackSlate();
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
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(HitMarkerText, STextBlock)
			.Text(FText::FromString(TEXT("X")))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
			.Visibility(EVisibility::Collapsed)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SAssignNew(DamageNumberOverlay, SOverlay)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.0f, 118.0f, 0.0f, 0.0f))
		[
			SAssignNew(LocalHitDirectionText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
			.ColorAndOpacity(FLinearColor(1.0f, 0.16f, 0.08f, 0.0f))
			.Visibility(EVisibility::Collapsed)
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
	AdvanceHitFeedback(InDeltaTime);
	RefreshText();
}

void UPRWeaponHUDWidget::RefreshHitFeedbackSlate()
{
	if (HitMarkerText)
	{
		HitMarkerText->SetVisibility(IsHitMarkerActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		FLinearColor MarkerDisplayColor = HitMarkerColor;
		MarkerDisplayColor.A = HitMarkerLifetimeSeconds > 0.0f
			? FMath::Clamp(HitMarkerRemainingSeconds / HitMarkerLifetimeSeconds, 0.0f, 1.0f)
			: 0.0f;
		HitMarkerText->SetColorAndOpacity(MarkerDisplayColor);
	}

	if (DamageNumberOverlay)
	{
		DamageNumberOverlay->ClearChildren();
		for (int32 Index = 0; Index < ActiveDamageNumbers.Num(); ++Index)
		{
			const FPRHUDDamageNumber& Number = ActiveDamageNumbers[Index];
			FLinearColor DisplayColor = Number.Color;
			DisplayColor.A *= FMath::Clamp(Number.RemainingSeconds / DamageNumberLifetimeSeconds, 0.0f, 1.0f);
			const float HorizontalOffset = Index % 2 == 0 ? -24.0f : 24.0f;
			DamageNumberOverlay->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(FMargin(HorizontalOffset, -54.0f - Number.DisplayOffset, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(FormatDamageAmount(Number.Amount))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
					.ColorAndOpacity(DisplayColor)
				];
		}
	}

	if (LocalHitDirectionText)
	{
		LocalHitDirectionText->SetVisibility(LocalHitPulseRemaining > 0.0f
			? EVisibility::HitTestInvisible
			: EVisibility::Collapsed);
		LocalHitDirectionText->SetText(ResolveDirectionIndicatorText());
		FLinearColor PulseColor(1.0f, 0.16f, 0.08f, FMath::Clamp(
			LocalHitPulseRemaining / LocalHitPulseLifetimeSeconds,
			0.0f,
			1.0f));
		LocalHitDirectionText->SetColorAndOpacity(PulseColor);
	}
}

FText UPRWeaponHUDWidget::ResolveDirectionIndicatorText() const
{
	const APlayerController* Controller = LocalController.Get();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn || LocalHitDirection.IsNearlyZero())
	{
		return FText::FromString(TEXT("!"));
	}

	const FVector Direction = LocalHitDirection.GetSafeNormal2D();
	const float ForwardAmount = FVector::DotProduct(Pawn->GetActorForwardVector(), Direction);
	const float RightAmount = FVector::DotProduct(Pawn->GetActorRightVector(), Direction);
	if (FMath::Abs(RightAmount) > FMath::Abs(ForwardAmount))
	{
		return FText::FromString(RightAmount >= 0.0f ? TEXT(">") : TEXT("<"));
	}
	return FText::FromString(ForwardAmount >= 0.0f ? TEXT("^") : TEXT("v"));
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
