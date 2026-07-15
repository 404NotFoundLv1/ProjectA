#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_PrimaryAttack.generated.h"

/**
 * Primary input ability that delegates server-authoritative firing to the equipped weapon component.
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
	bool ExecuteServerAttack(const FGameplayAbilityActorInfo* ActorInfo) const;
};
