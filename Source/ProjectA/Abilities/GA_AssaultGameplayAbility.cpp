#include "Abilities/GA_AssaultGameplayAbility.h"

#include "Core/PRGameplayTags.h"

UGA_AssaultGameplayAbility::UGA_AssaultGameplayAbility()
{
}

bool UGA_AssaultGameplayAbility::TryCommitAssaultAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	return TryCommitRoleModuleAbility(Handle, ActorInfo, ActivationInfo);
}
