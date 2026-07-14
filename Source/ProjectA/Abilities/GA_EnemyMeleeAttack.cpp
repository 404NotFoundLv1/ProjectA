#include "Abilities/GA_EnemyMeleeAttack.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Core/PRGameplayTags.h"
#include "GameplayAbilitySpec.h"

UGA_EnemyMeleeAttack::UGA_EnemyMeleeAttack()
{
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = ProjectRiftGameplayTags::Event_Ability_Enemy_Melee;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	TargetStatusEffects.Add(FPRTargetStatusEffectDefinition(
		UPRPollutionStatusGameplayEffect::StaticClass(),
		2.0f,
		5.0f));
}

bool UGA_EnemyMeleeAttack::IsTargetInRange(const AActor* SourceActor, const AActor* TargetActor) const
{
	return SourceActor
		&& TargetActor
		&& FVector::DistSquared(SourceActor->GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(AttackRange);
}

void UGA_EnemyMeleeAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AActor* SourceActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceAbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;
	UAbilitySystemComponent* TargetAbilitySystem = TargetActor
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor)
		: nullptr;

	bool bApplied = false;
	if (SourceActor
		&& TargetActor
		&& SourceAbilitySystem
		&& TargetAbilitySystem
		&& IsTargetInRange(SourceActor, TargetActor))
	{
		bApplied = UPRCombatEffectLibrary::ApplyDamageToTarget(
			SourceAbilitySystem,
			TargetAbilitySystem,
			BaseDamage,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			this);

		if (bApplied)
		{
			for (const FPRTargetStatusEffectDefinition& StatusDefinition : TargetStatusEffects)
			{
				UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
					SourceAbilitySystem,
					TargetAbilitySystem,
					StatusDefinition,
					this);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bApplied);
}
