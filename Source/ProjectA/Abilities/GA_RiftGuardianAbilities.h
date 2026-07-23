#pragma once

#include "Abilities/PRBossGameplayAbility.h"
#include "GA_RiftGuardianAbilities.generated.h"

UCLASS()
class PROJECTA_API UGA_RiftGuardianLockedCharge : public UPRBossGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

UCLASS()
class PROJECTA_API UGA_RiftGuardianPollutionField : public UPRBossGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
