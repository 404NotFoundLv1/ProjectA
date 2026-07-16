#include "Abilities/GA_MedicPurificationPulse.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "EngineUtils.h"

FGameplayTag UGA_MedicPurificationPulse::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_E; }
TSubclassOf<UGameplayEffect> UGA_MedicPurificationPulse::GetModuleCooldownEffectClass() const { return UPRAssaultBlastCooldownGameplayEffect::StaticClass(); }

void UGA_MedicPurificationPulse::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UPRMedicModuleDataAsset* Module = GetMedicModuleDefinition(Handle, ActorInfo, EPRMedicModuleKind::PurificationPulse);
	TArray<UAbilitySystemComponent*> Targets;
	if (!Module || !FMath::IsFinite(Module->EffectRadius) || Module->EffectRadius <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FindPurifiableTargets(ActorInfo, Module->EffectRadius, Targets);
	if (Targets.IsEmpty() || !TryCommitRoleModuleAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	for (UAbilitySystemComponent* TargetASC : Targets)
	{
		if (UPRCombatEffectLibrary::ClearPurifiableStatusEffects(TargetASC))
		{
			TargetASC->ExecuteGameplayCue(ProjectRiftGameplayTags::GameplayCue_Combat_Support_Purified, FGameplayCueParameters());
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_MedicPurificationPulse::FindPurifiableTargets(const FGameplayAbilityActorInfo* ActorInfo, const float Radius,
	TArray<UAbilitySystemComponent*>& OutTargets) const
{
	OutTargets.Reset();
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	APRCharacter* SourceCharacter = ActorInfo ? Cast<APRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = SourceCharacter ? SourceCharacter->GetWorld() : nullptr;
	if (!SourceASC || !SourceCharacter || !World) return;
	const float RadiusSquared = Radius * Radius;
	for (TActorIterator<APRCharacter> It(World); It; ++It)
	{
		APRCharacter* Candidate = *It;
		UAbilitySystemComponent* CandidateASC = Candidate ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate) : nullptr;
		if (!CandidateASC || FVector::DistSquared(SourceCharacter->GetActorLocation(), Candidate->GetActorLocation()) > RadiusSquared
			|| !UPRCombatEffectLibrary::IsFriendlyPlayerTarget(SourceASC, CandidateASC))
		{
			continue;
		}
		if (CandidateASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted)
			|| CandidateASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed)
			|| CandidateASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned))
		{
			OutTargets.Add(CandidateASC);
		}
	}
}
