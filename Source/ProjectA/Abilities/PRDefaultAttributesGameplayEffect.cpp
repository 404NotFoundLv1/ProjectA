#include "Abilities/PRDefaultAttributesGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"

namespace
{
void AddOverrideModifier(UGameplayEffect& GameplayEffect, const FGameplayAttribute& Attribute, const float Value)
{
	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = Attribute;
	ModifierInfo.ModifierOp = EGameplayModOp::Override;
	ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

	GameplayEffect.Modifiers.Add(ModifierInfo);
}
}

UPRDefaultAttributesGameplayEffect::UPRDefaultAttributesGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	AddOverrideModifier(*this, UPRAttributeSet::GetMaxHealthAttribute(), 100.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetHealthAttribute(), 100.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetMaxShieldAttribute(), 50.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetShieldAttribute(), 50.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetMaxEnergyAttribute(), 100.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetEnergyAttribute(), 100.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetAttackPowerAttribute(), 10.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetMoveSpeedAttribute(), 600.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetCooldownReductionAttribute(), 0.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetHealingPowerAttribute(), 0.0f);
	AddOverrideModifier(*this, UPRAttributeSet::GetPollutionResistanceAttribute(), 0.0f);
}
