#pragma once

#include "Abilities/PRGameplayAbility.h"
#include "PRBossGameplayAbility.generated.h"

/** Server-only base for small, data-selected Boss pattern abilities. */
UCLASS(Abstract)
class PROJECTA_API UPRBossGameplayAbility : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void ExecuteBossPattern(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) {}
};
