#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRRoleModuleGameplayAbility.h"
#include "GA_AssaultGameplayAbility.generated.h"

class UPRAttributeSet;
class UPRRoleModuleDataAsset;
class UGameplayEffect;

/**
 * Shared first-pass behavior for assault role abilities.
 */
UCLASS(Abstract)
class PROJECTA_API UGA_AssaultGameplayAbility : public UPRRoleModuleGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultGameplayAbility();

protected:
	bool TryCommitAssaultAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	virtual FGameplayTag GetModuleCooldownTag() const PURE_VIRTUAL(UGA_AssaultGameplayAbility::GetModuleCooldownTag, return FGameplayTag(););
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const PURE_VIRTUAL(UGA_AssaultGameplayAbility::GetModuleCooldownEffectClass, return nullptr;);
};
