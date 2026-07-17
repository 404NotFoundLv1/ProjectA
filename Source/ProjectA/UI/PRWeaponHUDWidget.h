#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Blueprint/UserWidget.h"
#include "PRWeaponHUDWidget.generated.h"

class APlayerController;
class SOverlay;
class STextBlock;

/** One short-lived, source-owned damage number rendered by the weapon HUD. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRHUDDamageNumber
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|UI")
	float Amount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|UI")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|UI")
	float RemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|UI")
	float DisplayOffset = 0.0f;
};

/** Native crosshair and ammo readout for the local weapon component. */
UCLASS()
class PROJECTA_API UPRWeaponHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void InitializeForController(APlayerController* InController);

	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void PushHitConfirmation(const FPRHitConfirmation& Confirmation);

	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void PushLocalHitFeedback(const FPRResolvedDamage& ResolvedDamage);

	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void AdvanceHitFeedback(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Weapon|UI")
	bool IsHitMarkerActive() const { return HitMarkerRemainingSeconds > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Weapon|UI")
	int32 GetActiveDamageNumberCount() const { return ActiveDamageNumbers.Num(); }

	UFUNCTION(BlueprintPure, Category = "Weapon|UI")
	float GetHitMarkerRemainingSeconds() const { return HitMarkerRemainingSeconds; }

	/** Number of currently replicated enemies carrying the team-visible recon status. */
	UFUNCTION(BlueprintPure, Category = "Weapon|UI")
	int32 GetRevealedEnemyCount() const;

	/** Contextual revive prompt/progress and the local solo rescue-drone status. */
	UFUNCTION(BlueprintPure, Category = "Revive|UI")
	FText GetReviveStatusText() const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|UI")
	TArray<FPRHUDDamageNumber> ActiveDamageNumbers;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|UI")
	float LocalHitPulseRemaining = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon|UI")
	FVector LocalHitDirection = FVector::ZeroVector;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshText();
	void RefreshHitFeedbackSlate();
	void RefreshReconMarkers();
	FText ResolveDirectionIndicatorText() const;

	TWeakObjectPtr<APlayerController> LocalController;
	TSharedPtr<STextBlock> CrosshairText;
	TSharedPtr<STextBlock> AmmoText;
	TSharedPtr<STextBlock> HitMarkerText;
	TSharedPtr<STextBlock> LocalHitDirectionText;
	TSharedPtr<STextBlock> ReviveStatusText;
	TSharedPtr<SOverlay> DamageNumberOverlay;
	TSharedPtr<SOverlay> ReconMarkerOverlay;
	float HitMarkerRemainingSeconds = 0.0f;
	FLinearColor HitMarkerColor = FLinearColor::White;
};
