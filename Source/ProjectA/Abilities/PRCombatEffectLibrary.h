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
