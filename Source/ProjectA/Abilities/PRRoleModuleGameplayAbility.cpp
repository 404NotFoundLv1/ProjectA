#include "Abilities/PRRoleModuleGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Roles/PRRoleModuleDataAsset.h"

UPRRoleModuleGameplayAbility::UPRRoleModuleGameplayAbility()
{
	ActivationBlockedTags.AddTag(ProjectRiftGameplayTags::State_AbilityDisrupted);
}

bool UPRRoleModuleGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !GetRoleModuleDefinition(Handle, ActorInfo))
	{
		return false;
	}
	return CheckCost(Handle, ActorInfo, OptionalRelevantTags) && IsCooldownReady(ActorInfo);
}

bool UPRRoleModuleGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRAttributeSet* Attributes = ASC ? ASC->GetSet<UPRAttributeSet>() : nullptr;
	return Module && Attributes && FMath::IsFinite(Module->EnergyCost) && Module->EnergyCost >= 0.0f
		&& Attributes->GetEnergy() >= Module->EnergyCost && Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UPRRoleModuleGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	if (!ASC || !Module || Module->EnergyCost <= 0.0f) return;
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UPRRoleModuleCostGameplayEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo), ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyDelta, -Module->EnergyCost);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UPRRoleModuleGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPRRoleModuleDataAsset* Module = GetRoleModuleDefinition(Handle, ActorInfo);
	const TSubclassOf<UGameplayEffect> CooldownClass = GetModuleCooldownEffectClass();
	if (!ASC || !Module || !CooldownClass || Module->CooldownSeconds <= 0.0f) return;
	const UPRAttributeSet* Attributes = ASC->GetSet<UPRAttributeSet>();
	const float Reduction = Attributes ? FMath::Clamp(Attributes->GetCooldownReduction(), 0.0f, 0.8f) : 0.0f;
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CooldownClass, GetAbilityLevel(Handle, ActorInfo), ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_CooldownDuration, Module->CooldownSeconds * (1.0f - Reduction));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

bool UPRRoleModuleGameplayAbility::TryCommitRoleModuleAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	return CanActivateAbility(Handle, ActorInfo) && CommitAbility(Handle, ActorInfo, ActivationInfo);
}

UPRAttributeSet* UPRRoleModuleGameplayAbility::GetSourceAttributes(const FGameplayAbilityActorInfo* ActorInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return ASC ? const_cast<UPRAttributeSet*>(ASC->GetSet<UPRAttributeSet>()) : nullptr;
}

const UPRRoleModuleDataAsset* UPRRoleModuleGameplayAbility::GetRoleModuleDefinition(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPRRoleModuleDataAsset* Module = Spec ? Cast<UPRRoleModuleDataAsset>(Spec->SourceObject.Get()) : nullptr;
	return Module && Module->GameplayAbilityClass == GetClass() ? Module : nullptr;
}

bool UPRRoleModuleGameplayAbility::IsCooldownReady(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTag Tag = GetModuleCooldownTag();
	return ASC && Tag.IsValid() && !ASC->HasMatchingGameplayTag(Tag);
}
