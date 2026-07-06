#include "Abilities/PRDamageGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"
#include "GameplayTagsManager.h"

UPRDamageGameplayEffect::UPRDamageGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataName = TEXT("Data.Damage");
	DamageMagnitude.DataTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage"), false);

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UPRAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageMagnitude);

	Modifiers.Add(DamageModifier);
}
