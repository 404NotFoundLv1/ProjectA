#include "Abilities/GA_AssaultShield.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRTemporaryShieldGameplayEffect.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffect.h"
#include "ProjectA.h"

UGA_AssaultShield::UGA_AssaultShield()
{
	ShieldEffectClass = UPRTemporaryShieldGameplayEffect::StaticClass();
}

FGameplayTag UGA_AssaultShield::GetModuleCooldownTag() const
{
	return ProjectRiftGameplayTags::Cooldown_Skill_R;
}

TSubclassOf<UGameplayEffect> UGA_AssaultShield::GetModuleCooldownEffectClass() const
{
	return UPRAssaultShieldCooldownGameplayEffect::StaticClass();
}

void UGA_AssaultShield::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!TryCommitAssaultAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ApplyShieldEffect(Handle, ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_AssaultShield::ApplyShieldEffect(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!SourceASC || !ShieldEffectClass)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle ShieldSpecHandle = SourceASC->MakeOutgoingSpec(
		ShieldEffectClass,
		GetAbilityLevel(Handle, ActorInfo),
		EffectContext);
	if (!ShieldSpecHandle.IsValid())
	{
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToSelf(*ShieldSpecHandle.Data.Get());
	UE_LOG(LogProjectA, Log, TEXT("Assault shield applied to %s."), *GetNameSafe(AvatarActor));
	return true;
}
