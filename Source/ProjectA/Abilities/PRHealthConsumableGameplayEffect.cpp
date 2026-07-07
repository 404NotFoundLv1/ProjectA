#include "Abilities/PRHealthConsumableGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPRHealthConsumableGameplayEffect::UPRHealthConsumableGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UPRAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(35.0f));

	Modifiers.Add(HealthModifier);
}
