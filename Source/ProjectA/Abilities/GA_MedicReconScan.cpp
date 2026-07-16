#include "Abilities/GA_MedicReconScan.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "EngineUtils.h"

FGameplayTag UGA_MedicReconScan::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_R; }
TSubclassOf<UGameplayEffect> UGA_MedicReconScan::GetModuleCooldownEffectClass() const { return UPRAssaultShieldCooldownGameplayEffect::StaticClass(); }

void UGA_MedicReconScan::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UPRMedicModuleDataAsset* Module = GetMedicModuleDefinition(Handle, ActorInfo, EPRMedicModuleKind::ReconScan);
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	APRCharacter* SourceCharacter = ActorInfo ? Cast<APRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Module || !SourceASC || !SourceCharacter || !FMath::IsFinite(Module->EffectRadius) || Module->EffectRadius <= 0.0f
		|| !FMath::IsFinite(Module->DurationSeconds) || Module->DurationSeconds <= 0.0f
		|| !TryCommitRoleModuleAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float RadiusSquared = Module->EffectRadius * Module->EffectRadius;
	if (UWorld* World = SourceCharacter->GetWorld())
	{
		for (TActorIterator<APREnemyCharacter> It(World); It; ++It)
		{
			APREnemyCharacter* Enemy = *It;
			UAbilitySystemComponent* TargetASC = Enemy ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy) : nullptr;
			if (TargetASC && FVector::DistSquared(SourceCharacter->GetActorLocation(), Enemy->GetActorLocation()) <= RadiusSquared)
			{
				UPRCombatEffectLibrary::ApplyReconRevealToTarget(SourceASC, TargetASC, Module->DurationSeconds, this);
			}
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
