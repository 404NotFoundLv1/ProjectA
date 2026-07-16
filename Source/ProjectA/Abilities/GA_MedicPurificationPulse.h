#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_MedicGameplayAbility.h"
#include "GA_MedicPurificationPulse.generated.h"

class UAbilitySystemComponent;

UCLASS()
class PROJECTA_API UGA_MedicPurificationPulse : public UGA_MedicGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;

private:
	void FindPurifiableTargets(const FGameplayAbilityActorInfo* ActorInfo, float Radius,
		TArray<UAbilitySystemComponent*>& OutTargets) const;
};
