#include "Abilities/GA_PrimaryAttack.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "Weapons/PRWeaponComponent.h"

UGA_PrimaryAttack::UGA_PrimaryAttack()
{
	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DeadStateTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DeadStateTag);
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	if (DownedStateTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DownedStateTag);
	}
}

bool UGA_PrimaryAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DeadStateTag.IsValid() && AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(DeadStateTag))
	{
		return false;
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	return !DownedStateTag.IsValid() || !AbilitySystemComponent || !AbilitySystemComponent->HasMatchingGameplayTag(DownedStateTag);
}

void UGA_PrimaryAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ExecuteServerAttack(ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_PrimaryAttack::ExecuteServerAttack(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const APRCharacter* Avatar = ActorInfo ? Cast<APRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	APRPlayerState* PlayerState = Avatar ? Avatar->GetPlayerState<APRPlayerState>() : nullptr;
	UPRWeaponComponent* WeaponComponent = PlayerState ? PlayerState->GetWeaponComponent() : nullptr;
	if (!WeaponComponent)
	{
		return false;
	}
	const EPRWeaponFireResult Result = WeaponComponent->TryFire();
	return Result == EPRWeaponFireResult::FiredHit || Result == EPRWeaponFireResult::FiredMiss;
}
