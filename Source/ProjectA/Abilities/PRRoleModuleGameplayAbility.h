#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "PRRoleModuleGameplayAbility.generated.h"

class UPRAttributeSet;
class UPRRoleModuleDataAsset;
class UGameplayEffect;

/** Shared server-authoritative cost, cooldown, and source-data behavior for role modules. */
UCLASS(Abstract)
class PROJECTA_API UPRRoleModuleGameplayAbility : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UPRRoleModuleGameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
	bool TryCommitRoleModuleAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	UPRAttributeSet* GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const;
	const UPRRoleModuleDataAsset* GetRoleModuleDefinition(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	virtual FGameplayTag GetModuleCooldownTag() const PURE_VIRTUAL(UPRRoleModuleGameplayAbility::GetModuleCooldownTag, return FGameplayTag(););
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const PURE_VIRTUAL(UPRRoleModuleGameplayAbility::GetModuleCooldownEffectClass, return nullptr;);
	bool IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const;
};
