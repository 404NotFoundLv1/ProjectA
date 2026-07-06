#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_AssaultGameplayAbility.generated.h"

class UPRAttributeSet;

/**
 * Shared first-pass behavior for assault role abilities.
 */
UCLASS(Abstract)
class PROJECTA_API UGA_AssaultGameplayAbility : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssaultGameplayAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	bool TryCommitAssaultAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	UPRAttributeSet* GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const;

	UPROPERTY(EditDefaultsOnly, Category = "Assault")
	float EnergyCost;

	UPROPERTY(EditDefaultsOnly, Category = "Assault")
	float CooldownSeconds;

private:
	bool IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const;
	double GetWorldTime(const FGameplayAbilityActorInfo* ActorInfo) const;

	double LastActivationTime;
};
