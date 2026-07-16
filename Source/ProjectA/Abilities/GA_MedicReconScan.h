#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_MedicGameplayAbility.h"
#include "GA_MedicReconScan.generated.h"

UCLASS()
class PROJECTA_API UGA_MedicReconScan : public UGA_MedicGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;
};
