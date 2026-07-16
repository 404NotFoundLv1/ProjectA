#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_AssaultGameplayAbility.h"
#include "GA_AssaultCharge.generated.h"

/**
 * Assault Q: server-authoritative short forward charge.
 */
UCLASS()
class PROJECTA_API UGA_AssaultCharge : public UGA_AssaultGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultCharge();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;

private:
	bool ExecuteCharge(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Charge")
	float ChargeDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Charge")
	float KnockbackRadius;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Charge")
	float KnockbackStrength;
};
