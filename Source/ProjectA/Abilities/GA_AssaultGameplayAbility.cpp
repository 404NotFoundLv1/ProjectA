#include "Abilities/GA_AssaultGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"

namespace
{
bool HasAnyBlockedLifeState(const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return true;
	}

	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DeadStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DeadStateTag))
	{
		return true;
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	return DownedStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DownedStateTag);
}
}

UGA_AssaultGameplayAbility::UGA_AssaultGameplayAbility()
{
	EnergyCost = 0.0f;
	CooldownSeconds = 0.0f;
	LastActivationTime = -1.0;

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

bool UGA_AssaultGameplayAbility::CanActivateAbility(
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
	if (HasAnyBlockedLifeState(AbilitySystemComponent))
	{
		return false;
	}

	const UPRAttributeSet* AttributeSet = AbilitySystemComponent ? AbilitySystemComponent->GetSet<UPRAttributeSet>() : nullptr;
	if (!AttributeSet || AttributeSet->GetEnergy() < EnergyCost)
	{
		return false;
	}

	return IsCooldownReady(ActorInfo);
}

bool UGA_AssaultGameplayAbility::TryCommitAssaultAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!CanActivateAbility(Handle, ActorInfo))
	{
		return false;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	UPRAttributeSet* AttributeSet = GetSourceAttributes(ActorInfo);
	if (!AttributeSet)
	{
		return false;
	}

	if (EnergyCost > 0.0f)
	{
		const float NewEnergy = FMath::Clamp(AttributeSet->GetEnergy() - EnergyCost, 0.0f, AttributeSet->GetMaxEnergy());
		AttributeSet->SetEnergy(NewEnergy);
	}

	LastActivationTime = GetWorldTime(ActorInfo);
	return true;
}

UPRAttributeSet* UGA_AssaultGameplayAbility::GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystemComponent ? const_cast<UPRAttributeSet*>(AbilitySystemComponent->GetSet<UPRAttributeSet>()) : nullptr;
}

bool UGA_AssaultGameplayAbility::IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (CooldownSeconds <= 0.0f || LastActivationTime < 0.0)
	{
		return true;
	}

	return GetWorldTime(ActorInfo) >= LastActivationTime + CooldownSeconds;
}

double UGA_AssaultGameplayAbility::GetWorldTime(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0;
}
