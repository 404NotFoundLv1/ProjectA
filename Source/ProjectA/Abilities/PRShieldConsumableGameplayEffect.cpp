#include "Abilities/PRShieldConsumableGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPRShieldConsumableGameplayEffect::UPRShieldConsumableGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ShieldModifier;
	ShieldModifier.Attribute = UPRAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(25.0f));

	Modifiers.Add(ShieldModifier);
}
