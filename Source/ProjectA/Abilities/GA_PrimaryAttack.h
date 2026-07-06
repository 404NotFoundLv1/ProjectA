#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_PrimaryAttack.generated.h"

class UGameplayEffect;
class UPRAbilitySystemComponent;

/**
 * First-pass GAS primary attack: server-side short range hit check, then damage GE.
 */
UCLASS()
class PROJECTA_API UGA_PrimaryAttack : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_PrimaryAttack();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	bool ExecuteServerAttack(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Primary Attack")
	float AttackRange;

	UPROPERTY(EditDefaultsOnly, Category = "Primary Attack")
	float AttackRadius;

	UPROPERTY(EditDefaultsOnly, Category = "Primary Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
