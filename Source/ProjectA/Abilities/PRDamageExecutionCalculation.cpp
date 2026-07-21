#include "Abilities/PRDamageExecutionCalculation.h"

#include "Abilities/PRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRRiftGameState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Settings/PRProjectSettings.h"

namespace
{
struct FPRDamageCaptureDefinitions
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
	DECLARE_ATTRIBUTE_CAPTUREDEF(PollutionResistance);

	FPRDamageCaptureDefinitions()
	{
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, AttackPower, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, PollutionResistance, Target, false);
	}
};

const FPRDamageCaptureDefinitions& GetDamageCaptureDefinitions()
{
	static const FPRDamageCaptureDefinitions Definitions;
	return Definitions;
}
}

UPRDamageExecutionCalculation::UPRDamageExecutionCalculation()
{
	const FPRDamageCaptureDefinitions& Definitions = GetDamageCaptureDefinitions();
	RelevantAttributesToCapture.Add(Definitions.AttackPowerDef);
	RelevantAttributesToCapture.Add(Definitions.PollutionResistanceDef);
}

void UPRDamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float BaseDamage = Spec.GetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Damage, false, 0.0f);
	if (!FMath::IsFinite(BaseDamage) || BaseDamage <= 0.0f)
	{
		return;
	}

	FGameplayTagContainer AssetTags;
	Spec.GetAllAssetTags(AssetTags);
	const bool bPhysicalDamage = AssetTags.HasTagExact(ProjectRiftGameplayTags::Damage_Type_Physical);
	const bool bPollutionDamage = AssetTags.HasTagExact(ProjectRiftGameplayTags::Damage_Type_Pollution);
	if (bPhysicalDamage && bPollutionDamage)
	{
		return;
	}

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	const FPRDamageCaptureDefinitions& Definitions = GetDamageCaptureDefinitions();
	float AttackPower = 0.0f;
	const bool bSelfSourced = ExecutionParams.GetSourceAbilitySystemComponent()
		== ExecutionParams.GetTargetAbilitySystemComponent();
	if (!bSelfSourced)
	{
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
			Definitions.AttackPowerDef,
			EvaluationParameters,
			AttackPower);
	}
	if (!FMath::IsFinite(AttackPower))
	{
		AttackPower = 0.0f;
	}

	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float AttackPowerDivisor = Settings
		? FMath::Max(Settings->AttackPowerDivisor, 1.0f)
		: 100.0f;
	float FinalDamage = BaseDamage * (1.0f + FMath::Max(AttackPower, 0.0f) / AttackPowerDivisor);

	if (bPollutionDamage)
	{
		float PollutionResistance = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
			Definitions.PollutionResistanceDef,
			EvaluationParameters,
			PollutionResistance);
		if (!FMath::IsFinite(PollutionResistance))
		{
			PollutionResistance = 0.0f;
		}

		const float MaxReduction = Settings
			? FMath::Clamp(Settings->MaxPollutionDamageReduction, 0.0f, 1.0f)
			: 0.8f;
		FinalDamage *= 1.0f - FMath::Clamp(PollutionResistance, 0.0f, MaxReduction);
		if (const UAbilitySystemComponent* TargetAbilitySystem = ExecutionParams.GetTargetAbilitySystemComponent())
		{
			if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Mission_Modifier_PollutionAmplified))
			{
				if (const AActor* TargetAvatar = TargetAbilitySystem->GetAvatarActor())
				{
					if (const APRRiftGameState* RiftGameState = TargetAvatar->GetWorld() ? TargetAvatar->GetWorld()->GetGameState<APRRiftGameState>() : nullptr)
					{
						FinalDamage *= RiftGameState->GetMissionRuleSnapshot().PollutionDamageMultiplier;
					}
				}
			}
		}
	}

	if (!FMath::IsFinite(FinalDamage) || FinalDamage <= 0.0f)
	{
		return;
	}

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UPRAttributeSet::GetDamageAttribute(),
		EGameplayModOp::Additive,
		FinalDamage));
}
