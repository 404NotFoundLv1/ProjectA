#include "Abilities/PRGameplayAbility.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/PRGameplayTags.h"

UPRGameplayAbility::UPRGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_Downed);
	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_HitStaggered);
	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_PlacingDeployable);
}

bool UPRGameplayAbility::ApplyConfiguredStatusEffects(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem) const
{
	bool bAppliedAnyStatus = false;
	for (const FPRTargetStatusEffectDefinition& Definition : TargetStatusEffects)
	{
		bAppliedAnyStatus |= UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			SourceAbilitySystem,
			TargetAbilitySystem,
			Definition,
			const_cast<UPRGameplayAbility*>(this)).IsValid();
	}
	return bAppliedAnyStatus;
}
