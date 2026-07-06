#include "Abilities/PRPrimaryAttackDamageGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPRPrimaryAttackDamageGameplayEffect::UPRPrimaryAttackDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UPRAttributeSet::GetHealthAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-10.0f));

	Modifiers.Add(DamageModifier);
}
