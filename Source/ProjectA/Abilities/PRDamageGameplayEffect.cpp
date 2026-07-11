#include "Abilities/PRDamageGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"

UPRDamageGameplayEffect::UPRDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataName = NAME_None;
	DamageMagnitude.DataTag = ProjectRiftGameplayTags::Data_Damage;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UPRAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);

	Modifiers.Add(DamageModifier);
}
