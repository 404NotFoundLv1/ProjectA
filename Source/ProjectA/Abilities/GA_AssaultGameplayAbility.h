#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_AssaultGameplayAbility.generated.h"

class UPRAttributeSet;
class UPRRoleModuleDataAsset;
class UGameplayEffect;

/**
 * Shared first-pass behavior for assault role abilities.
 */
UCLASS(Abstract)
class PROJECTA_API UGA_AssaultGameplayAbility : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultGameplayAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	bool TryCommitAssaultAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	UPRAttributeSet* GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const;
	const UPRRoleModuleDataAsset* GetRoleModuleDefinition(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	virtual FGameplayTag GetModuleCooldownTag() const PURE_VIRTUAL(UGA_AssaultGameplayAbility::GetModuleCooldownTag, return FGameplayTag(););
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const PURE_VIRTUAL(UGA_AssaultGameplayAbility::GetModuleCooldownEffectClass, return nullptr;);

	bool IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const;
};
