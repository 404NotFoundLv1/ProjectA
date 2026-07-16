#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "PRCombatFeedbackComponent.generated.h"

class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UParticleSystem;
class USkeletalMeshComponent;

/** Shared presentation endpoint for player and enemy combat feedback. */
UCLASS(ClassGroup = (ProjectRift), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRCombatFeedbackComponent();

	UFUNCTION(BlueprintPure, Category = "Combat|Feedback")
	static float ResolveCameraShakeScale(bool bReduceCameraShake);

	void RecordResolvedDamage(
		const FPRResolvedDamage& ResolvedDamage,
		const FGameplayEffectContextHandle& EffectContext);
	void HandleGameplayCue(
		FGameplayTag CueTag,
		EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters);

	FActiveGameplayEffectHandle GetActiveStaggerHandle() const { return ActiveStaggerHandle; }
	EPRHitReactionStrength GetActiveStaggerStrength() const { return ActiveStaggerStrength; }
	void SetActiveStagger(FActiveGameplayEffectHandle Handle, EPRHitReactionStrength Strength);
	void ClearActiveStagger();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FPRResolvedDamage LastResolvedDamage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FGameplayEffectContextHandle LastDamageEffectContext;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 FeedbackDispatchCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FGameplayTagContainer LastDamageCueTags;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FGameplayTag LastHandledCueTag;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 GameplayCueDispatchCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 GameplayCueHandledCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 LocalHitPulseCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 LastDamageFeedbackSequence = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	bool bLastImpactFromBack = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FGameplayTagContainer ActiveStatusCueTags;

private:
	USkeletalMeshComponent* ResolveSkeletalMesh() const;
	void PlayImpactPresentation(FGameplayTag CueTag, const FGameplayCueParameters& Parameters);
	void RefreshPersistentOverlay();
	void ApplyOverlayTint(const FLinearColor& TintColor, float DurationSeconds);
	void RestoreOverlayAfterImpact();
	void RestoreOriginalOverlay();
	void SetRevealPresentation(bool bEnabled);

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Animation")
	TSoftObjectPtr<UAnimMontage> FrontLightHitMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Animation")
	TSoftObjectPtr<UAnimMontage> FrontHeavyHitMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Animation")
	TSoftObjectPtr<UAnimMontage> BackHitMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Visual")
	TSoftObjectPtr<UMaterialInterface> FeedbackOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Visual")
	TSoftObjectPtr<UParticleSystem> ImpactParticleSystem;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalOverlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActiveOverlayMaterial;

	FTimerHandle OverlayTimerHandle;
	bool bHasOverriddenOverlay = false;
	bool bHasSavedRevealDepthState = false;
	bool bOriginalRenderCustomDepth = false;
	int32 OriginalCustomDepthStencilValue = 0;
	bool bResolvedDamagePulsePending = false;
	int32 LastPulsedFeedbackSequence = 0;
	FActiveGameplayEffectHandle ActiveStaggerHandle;
	EPRHitReactionStrength ActiveStaggerStrength = EPRHitReactionStrength::None;
};
