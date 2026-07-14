#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRStatusEffectTypes.h"
#include "GameplayEffectTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PRCombatEffectLibrary.generated.h"

class UAbilitySystemComponent;

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

	static bool ApplyDamageToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		float BaseDamage,
		FGameplayTag DamageType,
		UObject* SourceObject = nullptr);

	static FActiveGameplayEffectHandle ApplyStatusEffectToTarget(
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FPRTargetStatusEffectDefinition& Definition,
		UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ProjectRift|Combat")
	static void ClearNegativeStatusEffects(UAbilitySystemComponent* TargetAbilitySystem);

	UFUNCTION(BlueprintPure, Category = "ProjectRift|Combat")
	static FString GetActiveNegativeStatusText(const UAbilitySystemComponent* TargetAbilitySystem);
};
