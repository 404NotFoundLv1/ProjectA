#include "Abilities/PRAffixGameplayEffects.h"

#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"

namespace
{
FGameplayAttribute GetAttributeForAffix(const EPRAffixAttribute Attribute)
{
	switch (Attribute)
	{
	case EPRAffixAttribute::MaxHealth: return UPRAttributeSet::GetMaxHealthAttribute();
	case EPRAffixAttribute::MaxShield: return UPRAttributeSet::GetMaxShieldAttribute();
	case EPRAffixAttribute::MaxEnergy: return UPRAttributeSet::GetMaxEnergyAttribute();
	case EPRAffixAttribute::AttackPower: return UPRAttributeSet::GetAttackPowerAttribute();
	case EPRAffixAttribute::MoveSpeed: return UPRAttributeSet::GetMoveSpeedAttribute();
	case EPRAffixAttribute::HealingPower: return UPRAttributeSet::GetHealingPowerAttribute();
	case EPRAffixAttribute::PollutionResistance: return UPRAttributeSet::GetPollutionResistanceAttribute();
	default: return FGameplayAttribute();
	}
}

FGameplayEffectModifierMagnitude MakeAffixMagnitude()
{
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = ProjectRiftGameplayTags::Data_Affix_Magnitude;
	return FGameplayEffectModifierMagnitude(SetByCaller);
}
}

UPRAffixModifierGameplayEffect::UPRAffixModifierGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

void UPRAffixModifierGameplayEffect::Configure(const EPRAffixAttribute InAttribute, const EPRAffixModifierType InModifierType)
{
	AffixAttribute = InAttribute;
	AffixModifierType = InModifierType;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = GetAttributeForAffix(InAttribute);
	Modifier.ModifierOp = InModifierType == EPRAffixModifierType::Percentage
		? EGameplayModOp::Multiplicitive
		: EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = MakeAffixMagnitude();
}

#define PROJECTRIFT_DEFINE_AFFIX_EFFECT(ClassName, AttributeValue, TypeValue) \
	ClassName::ClassName() { Configure(AttributeValue, TypeValue); }

PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxHealthAdditiveGameplayEffect, EPRAffixAttribute::MaxHealth, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxHealthPercentageGameplayEffect, EPRAffixAttribute::MaxHealth, EPRAffixModifierType::Percentage)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxShieldAdditiveGameplayEffect, EPRAffixAttribute::MaxShield, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxShieldPercentageGameplayEffect, EPRAffixAttribute::MaxShield, EPRAffixModifierType::Percentage)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxEnergyAdditiveGameplayEffect, EPRAffixAttribute::MaxEnergy, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMaxEnergyPercentageGameplayEffect, EPRAffixAttribute::MaxEnergy, EPRAffixModifierType::Percentage)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixAttackPowerAdditiveGameplayEffect, EPRAffixAttribute::AttackPower, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixAttackPowerPercentageGameplayEffect, EPRAffixAttribute::AttackPower, EPRAffixModifierType::Percentage)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMoveSpeedAdditiveGameplayEffect, EPRAffixAttribute::MoveSpeed, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixMoveSpeedPercentageGameplayEffect, EPRAffixAttribute::MoveSpeed, EPRAffixModifierType::Percentage)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixHealingPowerAdditiveGameplayEffect, EPRAffixAttribute::HealingPower, EPRAffixModifierType::Additive)
PROJECTRIFT_DEFINE_AFFIX_EFFECT(UPRAffixPollutionResistanceAdditiveGameplayEffect, EPRAffixAttribute::PollutionResistance, EPRAffixModifierType::Additive)

#undef PROJECTRIFT_DEFINE_AFFIX_EFFECT
