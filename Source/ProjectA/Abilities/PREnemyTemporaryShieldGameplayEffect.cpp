#include "Abilities/PREnemyTemporaryShieldGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"

UPREnemyTemporaryShieldGameplayEffect::UPREnemyTemporaryShieldGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	FSetByCallerFloat Amount;
	Amount.DataTag = ProjectRiftGameplayTags::Data_Status_Magnitude;
	FGameplayModifierInfo MaxShield;
	MaxShield.Attribute = UPRAttributeSet::GetMaxShieldAttribute();
	MaxShield.ModifierOp = EGameplayModOp::Additive;
	MaxShield.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
	Modifiers.Add(MaxShield);
	FGameplayModifierInfo Shield;
	Shield.Attribute = UPRAttributeSet::GetShieldAttribute();
	Shield.ModifierOp = EGameplayModOp::Additive;
	Shield.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
	Modifiers.Add(Shield);
}
