#pragma once

#include "Abilities/PRBossGameplayAbility.h"
#include "GA_BossGameplayAbilities.generated.h"

UCLASS()
class PROJECTA_API UGA_BossTargetedDamage : public UPRBossGameplayAbility
{
	GENERATED_BODY()
	protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

UCLASS()
class PROJECTA_API UGA_BossRadialDamage : public UPRBossGameplayAbility
{
	GENERATED_BODY()
	protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

UCLASS()
class PROJECTA_API UGA_BossSummon : public UPRBossGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

UCLASS()
class PROJECTA_API UGA_BossArenaEvent : public UPRBossGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ExecuteBossPattern(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
