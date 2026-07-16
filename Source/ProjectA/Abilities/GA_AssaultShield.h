#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_AssaultGameplayAbility.h"
#include "GA_AssaultShield.generated.h"

class UGameplayEffect;

/**
 * Assault R: applies a short tactical shield GameplayEffect to self.
 */
UCLASS()
class PROJECTA_API UGA_AssaultShield : public UGA_AssaultGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultShield();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;

private:
	bool ApplyShieldEffect(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Shield")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;
};
