#include "UI/PRWeaponHUDWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/PRGameplayTags.h"
#include "Characters/PRCharacter.h"
#include "Enemies/PREnemyCharacter.h"
#include "Items/PRWeaponDataAsset.h"
#include "Player/PRPlayerState.h"
#include "Revive/PRRescueDroneActor.h"
#include "Revive/PRReviveComponent.h"
#include "Weapons/PRWeaponComponent.h"
#include "EngineUtils.h"
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

const TCHAR* GetDroneStatusLabel(const EPRRescueDroneState State)
{
	switch (State)
	{
	case EPRRescueDroneState::Ready: return TEXT("READY");
	case EPRRescueDroneState::Deployed: return TEXT("DEPLOYED");
	case EPRRescueDroneState::Spent: return TEXT("SPENT");
	default: return TEXT("UNAVAILABLE");
	}
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

int32 UPRWeaponHUDWidget::GetRevealedEnemyCount() const
{
	const APlayerController* Controller = LocalController.Get();
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 RevealedEnemyCount = 0;
	for (TActorIterator<APREnemyCharacter> It(World); It; ++It)
	{
		APREnemyCharacter* Enemy = *It;
		const UAbilitySystemComponent* EnemyASC = Enemy
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy)
			: nullptr;
		if (EnemyASC && EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Revealed))
		{
			++RevealedEnemyCount;
		}
	}
	return RevealedEnemyCount;
}

FText UPRWeaponHUDWidget::GetReviveStatusText() const
{
	const APlayerController* Controller = LocalController.Get();
	const APRCharacter* LocalCharacter = Controller ? Cast<APRCharacter>(Controller->GetPawn()) : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!LocalCharacter || !World)
	{
		return FText::GetEmpty();
	}

	EPRRescueDroneState DroneState = EPRRescueDroneState::Unavailable;
	for (TActorIterator<APRRescueDroneActor> It(World); It; ++It)
	{
		if (const APRRescueDroneActor* Drone = *It; Drone && Drone->GetAssignedPlayer() == LocalCharacter)
		{
			DroneState = Drone->GetDroneState();
			break;
		}
	}

	if (const UPRReviveComponent* LocalRevive = LocalCharacter->GetReviveComponent(); LocalCharacter->IsDowned() && LocalRevive)
	{
		return FText::FromString(FString::Printf(TEXT("DOWNED  %.1fs\nDRONE %s"),
			LocalRevive->GetBleedOutRemainingSeconds(), GetDroneStatusLabel(DroneState)));
	}

	for (TActorIterator<APRCharacter> It(World); It; ++It)
	{
		const APRCharacter* Candidate = *It;
		const UPRReviveComponent* Revive = Candidate ? Candidate->GetReviveComponent() : nullptr;
		if (!Revive)
		{
			continue;
		}
		if (Revive->GetCurrentReviver() == LocalCharacter)
		{
			return FText::FromString(FString::Printf(TEXT("REVIVING  %d%%"), FMath::RoundToInt(Revive->GetReviveProgress() * 100.0f)));
		}
		if (Revive->CanBeRevivedBy(LocalCharacter))
		{
			const int32 DistanceMeters = FMath::RoundToInt(FVector::Distance(LocalCharacter->GetActorLocation(), Candidate->GetActorLocation()) / 100.0f);
			return FText::FromString(FString::Printf(TEXT("HOLD INTERACT TO REVIVE  %dm"), DistanceMeters));
		}
	}

	return DroneState == EPRRescueDroneState::Ready
		? FText::FromString(TEXT("DRONE READY"))
		: FText::GetEmpty();
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
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
		[
			SAssignNew(ReconMarkerOverlay, SOverlay)
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
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 104.0f))
		[
			SAssignNew(ReviveStatusText, STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17))
			.ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 0.95f))
			.Justification(ETextJustify::Center)
		];
}

void UPRWeaponHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceHitFeedback(InDeltaTime);
	RefreshText();
	RefreshReconMarkers();
}

void UPRWeaponHUDWidget::RefreshReconMarkers()
{
	if (!ReconMarkerOverlay)
	{
		return;
	}

	ReconMarkerOverlay->ClearChildren();
	const APlayerController* Controller = LocalController.Get();
	const APawn* LocalPawn = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!Controller || !LocalPawn || !World)
	{
		return;
	}

	for (TActorIterator<APREnemyCharacter> It(World); It; ++It)
	{
		APREnemyCharacter* Enemy = *It;
		const UAbilitySystemComponent* EnemyASC = Enemy
			? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy)
			: nullptr;
		if (!Enemy || !EnemyASC || !EnemyASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Revealed))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!Controller->ProjectWorldLocationToScreen(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), ScreenPosition, true))
		{
			continue;
		}

		const int32 DistanceMeters = FMath::RoundToInt(FVector::Distance(LocalPawn->GetActorLocation(), Enemy->GetActorLocation()) / 100.0f);
		ReconMarkerOverlay->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(ScreenPosition.X, ScreenPosition.Y, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("RECON  %dm"), DistanceMeters)))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
				.ColorAndOpacity(FLinearColor(0.08f, 0.95f, 1.0f, 0.95f))
			];
	}
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
		}
		else
		{
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
	if (ReviveStatusText)
	{
		const FText StatusText = GetReviveStatusText();
		ReviveStatusText->SetText(StatusText);
		ReviveStatusText->SetVisibility(StatusText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	}
}
