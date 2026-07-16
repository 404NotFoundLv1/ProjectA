#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_MedicGameplayAbility.h"
#include "GA_MedicFieldHeal.generated.h"

class UAbilitySystemComponent;

UCLASS()
class PROJECTA_API UGA_MedicFieldHeal : public UGA_MedicGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MedicFieldHeal();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;

private:
	UAbilitySystemComponent* ResolveHealingTarget(const FGameplayAbilityActorInfo* ActorInfo,
		const UPRMedicModuleDataAsset* Module) const;

	UPROPERTY(EditDefaultsOnly, Category = "Medic|Heal", meta = (ClampMin = "1.0"))
	float AimAssistRadius = 75.0f;
};
