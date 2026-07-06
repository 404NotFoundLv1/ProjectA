#include "Abilities/PRTemporaryShieldGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPRTemporaryShieldGameplayEffect::UPRTemporaryShieldGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));

	FGameplayModifierInfo ShieldModifier;
	ShieldModifier.Attribute = UPRAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));

	Modifiers.Add(ShieldModifier);
}
