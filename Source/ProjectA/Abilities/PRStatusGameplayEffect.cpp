#include "Abilities/PRStatusGameplayEffect.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRDamageExecutionCalculation.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UPRStatusGameplayEffect::UPRStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
}

UPRPollutionStatusGameplayEffect::UPRPollutionStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Keep the sixth callback strictly beyond the five-second expiry boundary.
	Period = FScalableFloat(1.0f + KINDA_SMALL_NUMBER);
	bExecutePeriodicEffectOnApplication = true;
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this,
		TEXT("PollutionTargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::Status_Debuff_Polluted);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UPRDamageExecutionCalculation::StaticClass();
	Executions.Add(DamageExecution);
	GameplayCues.Add(FGameplayEffectCue(
		ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted,
		0.0f,
		1.0f));
}

FGameplayTag UPRPollutionStatusGameplayEffect::GetStatusTag() const
{
	return ProjectRiftGameplayTags::Status_Debuff_Polluted;
}

UPRSlowStatusGameplayEffect::UPRSlowStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this,
		TEXT("SlowTargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::Status_Debuff_Slowed);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);

	FSetByCallerFloat SlowMagnitude;
	SlowMagnitude.DataName = NAME_None;
	SlowMagnitude.DataTag = ProjectRiftGameplayTags::Data_Status_Magnitude;

	FGameplayModifierInfo MoveSpeedModifier;
	MoveSpeedModifier.Attribute = UPRAttributeSet::GetMoveSpeedAttribute();
	MoveSpeedModifier.ModifierOp = EGameplayModOp::Multiplicitive;
	MoveSpeedModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SlowMagnitude);
	Modifiers.Add(MoveSpeedModifier);
	GameplayCues.Add(FGameplayEffectCue(
		ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed,
		0.0f,
		1.0f));
}

FGameplayTag UPRSlowStatusGameplayEffect::GetStatusTag() const
{
	return ProjectRiftGameplayTags::Status_Debuff_Slowed;
}

UPRStunStatusGameplayEffect::UPRStunStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this,
		TEXT("StunTargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::State_Stunned);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);
	GameplayCues.Add(FGameplayEffectCue(
		ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned,
		0.0f,
		1.0f));
}

FGameplayTag UPRStunStatusGameplayEffect::GetStatusTag() const
{
	return ProjectRiftGameplayTags::State_Stunned;
}

UPRReconRevealGameplayEffect::UPRReconRevealGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* TargetTagsComponent = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
		this,
		TEXT("RevealTargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::Status_Debuff_Revealed);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);
	GameplayCues.Add(FGameplayEffectCue(
		ProjectRiftGameplayTags::GameplayCue_Combat_Status_Revealed,
		0.0f,
		1.0f));
}

FGameplayTag UPRReconRevealGameplayEffect::GetStatusTag() const
{
	return ProjectRiftGameplayTags::Status_Debuff_Revealed;
}

UPRParasitizedStatusGameplayEffect::UPRParasitizedStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = true;
	UTargetTagsGameplayEffectComponent* Tags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(this, TEXT("ParasitizedTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::Status_Debuff_Parasitized);
	Tags->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(Tags);
	FGameplayModifierInfo EnergyDrain;
	EnergyDrain.Attribute = UPRAttributeSet::GetEnergyAttribute();
	EnergyDrain.ModifierOp = EGameplayModOp::Additive;
	EnergyDrain.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-6.0f));
	Modifiers.Add(EnergyDrain);
	FGameplayModifierInfo Slow;
	Slow.Attribute = UPRAttributeSet::GetMoveSpeedAttribute();
	Slow.ModifierOp = EGameplayModOp::Multiplicitive;
	Slow.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.85f));
	Modifiers.Add(Slow);
}

FGameplayTag UPRParasitizedStatusGameplayEffect::GetStatusTag() const { return ProjectRiftGameplayTags::Status_Debuff_Parasitized; }

UPRAbilityDisruptedStatusGameplayEffect::UPRAbilityDisruptedStatusGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* Tags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(this, TEXT("AbilityDisruptedTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::State_AbilityDisrupted);
	Tags->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(Tags);
}

FGameplayTag UPRAbilityDisruptedStatusGameplayEffect::GetStatusTag() const { return ProjectRiftGameplayTags::State_AbilityDisrupted; }

UPRDisruptionGraceGameplayEffect::UPRDisruptionGraceGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* Tags = ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(this, TEXT("DisruptionGraceTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::Status_Grace_Disruption);
	Tags->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(Tags);
}

FGameplayTag UPRDisruptionGraceGameplayEffect::GetStatusTag() const { return ProjectRiftGameplayTags::Status_Grace_Disruption; }
