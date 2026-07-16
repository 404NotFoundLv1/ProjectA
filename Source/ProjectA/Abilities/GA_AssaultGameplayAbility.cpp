#include "Abilities/GA_AssaultGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Roles/PRRoleModuleDataAsset.h"

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

	if (!GetRoleModuleDefinition(Handle, ActorInfo))
	{
		return false;
	}

	return CheckCost(Handle, ActorInfo, OptionalRelevantTags) && IsCooldownReady(ActorInfo);
}

bool UGA_AssaultGameplayAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRAttributeSet* AttributeSet = AbilitySystemComponent ? AbilitySystemComponent->GetSet<UPRAttributeSet>() : nullptr;
	if (!Module || !AttributeSet || !FMath::IsFinite(Module->EnergyCost) || Module->EnergyCost < 0.0f || AttributeSet->GetEnergy() < Module->EnergyCost)
	{
		return false;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UGA_AssaultGameplayAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	if (!AbilitySystemComponent || !Module || Module->EnergyCost <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle CostSpec = AbilitySystemComponent->MakeOutgoingSpec(
		UPRRoleModuleCostGameplayEffect::StaticClass(),
		GetAbilityLevel(Handle, ActorInfo),
		AbilitySystemComponent->MakeEffectContext());
	if (CostSpec.IsValid())
	{
		CostSpec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyDelta, -Module->EnergyCost);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
	}
}

void UGA_AssaultGameplayAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	const TSubclassOf<UGameplayEffect> CooldownEffectClass = GetModuleCooldownEffectClass();
	if (!AbilitySystemComponent || !Module || !CooldownEffectClass || Module->CooldownSeconds <= 0.0f)
	{
		return;
	}
	const UPRAttributeSet* Attributes = AbilitySystemComponent->GetSet<UPRAttributeSet>();
	const float CooldownReduction = Attributes ? FMath::Clamp(Attributes->GetCooldownReduction(), 0.0f, 0.8f) : 0.0f;
	const float CooldownDuration = Module->CooldownSeconds * (1.0f - CooldownReduction);
	FGameplayEffectSpecHandle CooldownSpec = AbilitySystemComponent->MakeOutgoingSpec(
		CooldownEffectClass,
		GetAbilityLevel(Handle, ActorInfo),
		AbilitySystemComponent->MakeEffectContext());
	if (CooldownSpec.IsValid())
	{
		CooldownSpec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_CooldownDuration, CooldownDuration);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
	}
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

	return true;
}

UPRAttributeSet* UGA_AssaultGameplayAbility::GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystemComponent ? const_cast<UPRAttributeSet*>(AbilitySystemComponent->GetSet<UPRAttributeSet>()) : nullptr;
}

const UPRRoleModuleDataAsset* UGA_AssaultGameplayAbility::GetRoleModuleDefinition(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPRRoleModuleDataAsset* Module = Spec ? Cast<UPRRoleModuleDataAsset>(Spec->SourceObject.Get()) : nullptr;
	return Module && Module->GameplayAbilityClass == GetClass() ? Module : nullptr;
}

bool UGA_AssaultGameplayAbility::IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTag CooldownTag = GetModuleCooldownTag();
	return AbilitySystemComponent && CooldownTag.IsValid() && !AbilitySystemComponent->HasMatchingGameplayTag(CooldownTag);
}
