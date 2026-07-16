#include "Abilities/GA_MedicFieldHeal.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

UGA_MedicFieldHeal::UGA_MedicFieldHeal()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

FGameplayTag UGA_MedicFieldHeal::GetModuleCooldownTag() const { return ProjectRiftGameplayTags::Cooldown_Skill_Q; }
TSubclassOf<UGameplayEffect> UGA_MedicFieldHeal::GetModuleCooldownEffectClass() const { return UPRAssaultChargeCooldownGameplayEffect::StaticClass(); }

void UGA_MedicFieldHeal::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const UPRMedicModuleDataAsset* Module = GetMedicModuleDefinition(Handle, ActorInfo, EPRMedicModuleKind::FieldHeal);
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UAbilitySystemComponent* TargetASC = ResolveHealingTarget(ActorInfo, Module);
	if (!Module || !SourceASC || !TargetASC || !TryCommitRoleModuleAbility(Handle, ActorInfo, ActivationInfo)
		|| !UPRCombatEffectLibrary::ApplyHealthHealingToTarget(SourceASC, TargetASC, Module->PrimaryMagnitude, this))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TargetASC->ExecuteGameplayCue(ProjectRiftGameplayTags::GameplayCue_Combat_Support_Healed, FGameplayCueParameters());
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UAbilitySystemComponent* UGA_MedicFieldHeal::ResolveHealingTarget(const FGameplayAbilityActorInfo* ActorInfo,
	const UPRMedicModuleDataAsset* Module) const
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	APRCharacter* SourceCharacter = ActorInfo ? Cast<APRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!SourceASC || !SourceCharacter || !Module || !FMath::IsFinite(Module->TargetRange) || Module->TargetRange <= 0.0f)
	{
		return nullptr;
	}

	FVector ViewLocation = SourceCharacter->GetPawnViewLocation();
	FRotator ViewRotation = SourceCharacter->GetControlRotation();
	if (const APlayerController* Controller = Cast<APlayerController>(ActorInfo->PlayerController.Get()))
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const FVector EndLocation = ViewLocation + ViewRotation.Vector() * Module->TargetRange;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRMedicFieldHeal), false, SourceCharacter);
	TArray<FHitResult> Hits;
	if (UWorld* World = SourceCharacter->GetWorld())
	{
		World->SweepMultiByObjectType(Hits, ViewLocation, EndLocation, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(AimAssistRadius), QueryParams);
		for (const FHitResult& Hit : Hits)
		{
			APRCharacter* Candidate = Cast<APRCharacter>(Hit.GetActor());
			UAbilitySystemComponent* CandidateASC = Candidate ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate) : nullptr;
			const UPRAttributeSet* CandidateAttributes = CandidateASC ? CandidateASC->GetSet<UPRAttributeSet>() : nullptr;
			if (!CandidateASC || !CandidateAttributes || CandidateAttributes->GetHealth() >= CandidateAttributes->GetMaxHealth()
				|| !UPRCombatEffectLibrary::IsFriendlyPlayerTarget(SourceASC, CandidateASC))
			{
				continue;
			}
			FHitResult Blocker;
			if (World->LineTraceSingleByChannel(Blocker, SourceCharacter->GetActorLocation(), Candidate->GetActorLocation(), ECC_Visibility, QueryParams)
				&& Blocker.GetActor() != Candidate)
			{
				continue;
			}
			return CandidateASC;
		}
	}

	const UPRAttributeSet* SourceAttributes = SourceASC->GetSet<UPRAttributeSet>();
	return SourceAttributes && SourceAttributes->GetHealth() < SourceAttributes->GetMaxHealth() ? SourceASC : nullptr;
}
