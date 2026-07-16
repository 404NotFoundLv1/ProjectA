#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_AssaultGameplayAbility.h"
#include "Deployables/PRDeployableTypes.h"
#include "GA_EngineerDeployableAbility.generated.h"

class UPREngineerModuleDataAsset;

UCLASS(Abstract)
class PROJECTA_API UGA_EngineerDeployableAbility : public UGA_AssaultGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	virtual EPRDeployableKind GetDeployableKind() const PURE_VIRTUAL(UGA_EngineerDeployableAbility::GetDeployableKind, return EPRDeployableKind::None;);

	const UPREngineerModuleDataAsset* GetEngineerModuleDefinition(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
};
