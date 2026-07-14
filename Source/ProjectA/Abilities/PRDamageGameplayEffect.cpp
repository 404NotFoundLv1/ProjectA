#include "Abilities/PRDamageGameplayEffect.h"

#include "Abilities/PRDamageExecutionCalculation.h"

UPRDamageGameplayEffect::UPRDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UPRDamageExecutionCalculation::StaticClass();
	Executions.Add(DamageExecution);
}
