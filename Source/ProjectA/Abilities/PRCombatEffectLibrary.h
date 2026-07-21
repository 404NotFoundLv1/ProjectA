#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRStatusEffectTypes.h"
#include "GameplayEffectTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PRCombatEffectLibrary.generated.h"

class UAbilitySystemComponent;
class UPRAttributeSet;

/** Shared, fail-closed combat entry points for damage and negative statuses. */
UCLASS()
class PROJECTA_API UPRCombatEffectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ProjectRift|Combat")
	static bool IsHostileTarget(
		const UAbilitySystemComponent* SourceAbilitySystem,
		const UAbilitySystemComponent* TargetAbilitySystem);

	/** Damage-only validation; permits player fire against v0.8.1 destroy objectives without opening status/friendly-fire paths. */
	static bool IsDamageableTarget(
		const UAbilitySystemComponent* SourceAbilitySystem,
		const UAbilitySystemComponent* TargetAbilitySystem);

	/** Friendly-only support validation; intentionally rejects enemies and inactive players. */
	static bool IsFriendlyPlayerTarget(
		const UAbilitySystemComponent* SourceAbilitySystem,
		const UAbilitySystemComponent* TargetAbilitySystem);

	/** Applies server-authoritative shield repair using the source HealingPower multiplier. */
	static bool ApplyShieldRepairToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		float BaseRepair,
		UObject* SourceObject = nullptr);

	/** Applies server-authoritative health restoration using the source HealingPower multiplier. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProjectRift|Combat")
	static bool ApplyHealthHealingToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		float BaseHealing,
		UObject* SourceObject = nullptr);

	/** Applies the non-stacking shield-generator aura to an eligible friendly player. */
	static bool ApplyShieldGeneratorAuraToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		UObject* SourceObject = nullptr);

	static bool ApplyDamageToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		float BaseDamage,
		FGameplayTag DamageType,
		UObject* SourceObject = nullptr);

	/** Server-owned environmental damage.  Self-sourcing deliberately disables AttackPower in the shared execution. */
	static bool ApplyEnvironmentalDamageToTarget(
		UAbilitySystemComponent* TargetAbilitySystem,
		float BaseDamage,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProjectRift|Combat")
	static bool ApplyDamageRequestToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FPRDamageRequest& DamageRequest,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProjectRift|Combat")
	static void DispatchResolvedDamageFeedback(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FPRDamageRequest& DamageRequest,
		const FPRResolvedDamage& ResolvedDamage,
		const FGameplayEffectContextHandle& EffectContext);

	static FActiveGameplayEffectHandle ApplyStatusEffectToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FPRTargetStatusEffectDefinition& Definition,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ProjectRift|Combat")
	static void ClearNegativeStatusEffects(UAbilitySystemComponent* TargetAbilitySystem);

	/** Clears only Medic-purifiable statuses: pollution, slow, and stun. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProjectRift|Combat")
	static bool ClearPurifiableStatusEffects(UAbilitySystemComponent* TargetAbilitySystem);

	/** Applies or refreshes a single hostile recon reveal status. */
	static FActiveGameplayEffectHandle ApplyReconRevealToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		float DurationSeconds,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintPure, Category = "ProjectRift|Combat")
	static FString GetActiveNegativeStatusText(const UAbilitySystemComponent* TargetAbilitySystem);

private:
	friend class UPRAttributeSet;

	static void DispatchResolvedDamageFeedbackInternal(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FPRDamageRequest& DamageRequest,
		const FPRResolvedDamage& ResolvedDamage,
		const FGameplayEffectContextHandle& EffectContext,
		bool bAllowPendingLethalTransition);
};
