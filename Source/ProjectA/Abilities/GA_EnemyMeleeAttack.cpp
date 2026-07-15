#include "Abilities/GA_EnemyMeleeAttack.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
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
	HitReaction.Strength = EPRHitReactionStrength::Heavy;
	HitReaction.DurationSeconds = 0.30f;
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
		const FVector TargetLocation = TargetActor->GetActorLocation();
		const FVector ToSource = SourceActor->GetActorLocation() - TargetLocation;
		const FVector ImpactNormal = ToSource.IsNearlyZero()
			? -SourceActor->GetActorForwardVector()
			: ToSource.GetSafeNormal();
		FHitResult SyntheticHit(
			TargetActor,
			Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()),
			TargetLocation,
			ImpactNormal);
		SyntheticHit.bBlockingHit = true;

		FPRDamageRequest DamageRequest;
		DamageRequest.BaseDamage = BaseDamage;
		DamageRequest.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
		DamageRequest.HitResult = SyntheticHit;
		DamageRequest.HitReaction = HitReaction;
		DamageRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
		bApplied = UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			SourceAbilitySystem,
			TargetAbilitySystem,
			DamageRequest,
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
