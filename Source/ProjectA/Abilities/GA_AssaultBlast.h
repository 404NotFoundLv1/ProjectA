#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/GA_AssaultGameplayAbility.h"
#include "GA_AssaultBlast.generated.h"

class UGameplayEffect;

/**
 * Assault E: server-side forward area blast that applies GE_Damage.
 */
UCLASS()
class PROJECTA_API UGA_AssaultBlast : public UGA_AssaultGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultBlast();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;

private:
	bool ExecuteBlast(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Blast")
	float BlastRange;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Blast")
	float BlastRadius;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Blast")
	float DamageAmount;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Blast")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Assault|Blast")
	FPRHitReactionDefinition HitReaction;
};
