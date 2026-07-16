#include "Abilities/PRTemporaryShieldGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPRTemporaryShieldGameplayEffect::UPRTemporaryShieldGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(6.0f));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;

	FGameplayModifierInfo MaxShieldModifier;
	MaxShieldModifier.Attribute = UPRAttributeSet::GetMaxShieldAttribute();
	MaxShieldModifier.ModifierOp = EGameplayModOp::Additive;
	MaxShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));
	Modifiers.Add(MaxShieldModifier);

	FGameplayModifierInfo ShieldModifier;
	ShieldModifier.Attribute = UPRAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));

	Modifiers.Add(ShieldModifier);
}
