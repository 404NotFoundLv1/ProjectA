#include "Abilities/PRRoleModuleGameplayEffects.h"

#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
FGameplayEffectModifierMagnitude MakeSetByCallerMagnitude(const FGameplayTag& DataTag)
{
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = NAME_None;
	Magnitude.DataTag = DataTag;
	return FGameplayEffectModifierMagnitude(Magnitude);
}
}

UPRRoleModuleCostGameplayEffect::UPRRoleModuleCostGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo EnergyModifier;
	EnergyModifier.Attribute = UPRAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyDelta);
	Modifiers.Add(EnergyModifier);
}

UPRRoleEnergyRegenGameplayEffect::UPRRoleEnergyRegenGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;
	UTargetTagRequirementsGameplayEffectComponent* TagRequirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("RoleRegenOngoingTags"));
	FGameplayTagRequirements OngoingRequirements;
	OngoingRequirements.IgnoreTags.AddTag(ProjectRiftGameplayTags::State_Dead);
	OngoingRequirements.IgnoreTags.AddTag(ProjectRiftGameplayTags::State_Downed);
	TagRequirements->OngoingTagRequirements = OngoingRequirements;
	GEComponents.Add(TagRequirements);
	FGameplayModifierInfo EnergyModifier;
	EnergyModifier.Attribute = UPRAttributeSet::GetEnergyAttribute();
	EnergyModifier.ModifierOp = EGameplayModOp::Additive;
	EnergyModifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyRegen);
	Modifiers.Add(EnergyModifier);
}

UPRRoleModuleCooldownGameplayEffect::UPRRoleModuleCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = MakeSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_CooldownDuration);
}

void UPRRoleModuleCooldownGameplayEffect::AddCooldownTag(
	const FObjectInitializer& ObjectInitializer,
	const FGameplayTag& CooldownTag,
	const FName& ComponentName)
{
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(this, ComponentName);
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(CooldownTag);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);
}

UPRAssaultChargeCooldownGameplayEffect::UPRAssaultChargeCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AddCooldownTag(ObjectInitializer, ProjectRiftGameplayTags::Cooldown_Skill_Q, TEXT("ChargeCooldownTags"));
}

UPRAssaultBlastCooldownGameplayEffect::UPRAssaultBlastCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AddCooldownTag(ObjectInitializer, ProjectRiftGameplayTags::Cooldown_Skill_E, TEXT("BlastCooldownTags"));
}

UPRAssaultShieldCooldownGameplayEffect::UPRAssaultShieldCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AddCooldownTag(ObjectInitializer, ProjectRiftGameplayTags::Cooldown_Skill_R, TEXT("ShieldCooldownTags"));
}

UPRShieldRepairGameplayEffect::UPRShieldRepairGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& ShieldModifier = Modifiers.AddDefaulted_GetRef();
	ShieldModifier.Attribute = UPRAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = MakeSetByCallerMagnitude(ProjectRiftGameplayTags::Data_ShieldRepair);
}

UPRShieldGeneratorAuraGameplayEffect::UPRShieldGeneratorAuraGameplayEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.55f));
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	FGameplayModifierInfo& MaxShieldModifier = Modifiers.AddDefaulted_GetRef();
	MaxShieldModifier.Attribute = UPRAttributeSet::GetMaxShieldAttribute();
	MaxShieldModifier.ModifierOp = EGameplayModOp::Additive;
	MaxShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));
	FGameplayModifierInfo& ShieldModifier = Modifiers.AddDefaulted_GetRef();
	ShieldModifier.Attribute = UPRAttributeSet::GetShieldAttribute();
	ShieldModifier.ModifierOp = EGameplayModOp::Additive;
	ShieldModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(30.0f));
}
