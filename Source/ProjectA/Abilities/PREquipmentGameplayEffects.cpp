#include "Abilities/PREquipmentGameplayEffects.h"

#include "Abilities/PRAttributeSet.h"

namespace
{
void AddPersistentModifier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute, const float Magnitude)
{
	FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
}
}

UPRTestArmorEquipmentGameplayEffect::UPRTestArmorEquipmentGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddPersistentModifier(*this, UPRAttributeSet::GetMaxHealthAttribute(), 25.0f);
}

UPRTestChipEquipmentGameplayEffect::UPRTestChipEquipmentGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddPersistentModifier(*this, UPRAttributeSet::GetAttackPowerAttribute(), 10.0f);
}

UPRTestToolEquipmentGameplayEffect::UPRTestToolEquipmentGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddPersistentModifier(*this, UPRAttributeSet::GetHealingPowerAttribute(), 20.0f);
}
