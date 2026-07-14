#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_EnemyMeleeAttack.generated.h"

/** Server-only enemy melee ability triggered by Event.Ability.Enemy.Melee. */
UCLASS()
class PROJECTA_API UGA_EnemyMeleeAttack : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EnemyMeleeAttack();
	float GetAttackRange() const { return AttackRange; }
	float GetBaseDamage() const { return BaseDamage; }
	bool IsTargetInRange(const AActor* SourceActor, const AActor* TargetActor) const;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Melee", meta = (ClampMin = "0.0"))
	float AttackRange = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Melee", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;
};
