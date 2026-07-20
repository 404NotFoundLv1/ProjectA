#include "Abilities/PREnergyConsumableGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

UPREnergyConsumableGameplayEffect::UPREnergyConsumableGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo EnergyModifier;
	EnergyModifier.Attribute = UPRAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));
	Modifiers.Add(EnergyModifier);
}
