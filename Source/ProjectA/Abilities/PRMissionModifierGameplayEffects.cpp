#include "Abilities/PRMissionModifierGameplayEffects.h"

#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UPRMissionModifierGameplayEffect::UPRMissionModifierGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

void UPRMissionModifierGameplayEffect::AddRuleTag(const FGameplayTag& Tag, const FName& ComponentName)
{
	UTargetTagsGameplayEffectComponent* TagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(ComponentName);
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(Tag);
	TagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TagsComponent);
}

UPRLowGravityMissionGameplayEffect::UPRLowGravityMissionGameplayEffect()
{
	AddRuleTag(ProjectRiftGameplayTags::Mission_Modifier_LowGravity, TEXT("LowGravityRuleTag"));
}

UPRShieldInterferenceMissionGameplayEffect::UPRShieldInterferenceMissionGameplayEffect()
{
	AddRuleTag(ProjectRiftGameplayTags::Mission_Modifier_ShieldInterference, TEXT("ShieldInterferenceRuleTag"));
	FGameplayModifierInfo& MaxShieldModifier = Modifiers.AddDefaulted_GetRef();
	MaxShieldModifier.Attribute = UPRAttributeSet::GetMaxShieldAttribute();
	MaxShieldModifier.ModifierOp = EGameplayModOp::Multiplicitive;
	MaxShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.65f));
}

UPRPollutionAmplifiedMissionGameplayEffect::UPRPollutionAmplifiedMissionGameplayEffect()
{
	AddRuleTag(ProjectRiftGameplayTags::Mission_Modifier_PollutionAmplified, TEXT("PollutionAmplifiedRuleTag"));
}

UPREliteReinforcementMissionGameplayEffect::UPREliteReinforcementMissionGameplayEffect()
{
	AddRuleTag(ProjectRiftGameplayTags::State_Enemy_Elite, TEXT("EliteReinforcementTag"));
}
