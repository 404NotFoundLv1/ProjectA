#pragma once

#include "CoreMinimal.h"
#include "PRRiftRuleTypes.generated.h"

UENUM(BlueprintType)
enum class EPRRiftRiskTier : uint8
{
	Stable,
	Unstable,
	Critical,
	Collapse
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRiftRiskBandDefinition
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	EPRRiftRiskTier Tier = EPRRiftRiskTier::Stable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MinimumStability = 75.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk", meta = (ClampMin = "0.0"))
	float EnemyBudgetMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk", meta = (ClampMin = "0.0"))
	float RewardMultiplier = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk", meta = (ClampMin = "0.0"))
	float EnvironmentalPollutionDamage = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Rift|Risk", meta = (ClampMin = "0.0"))
	float EnvironmentalPulseIntervalSeconds = 0.0f;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRiftRiskSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float Stability = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	EPRRiftRiskTier CurrentTier = EPRRiftRiskTier::Stable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	EPRRiftRiskTier PeakTier = EPRRiftRiskTier::Stable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float EnemyBudgetMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float RewardMultiplier = 1.0f;

	/** Reward multiplier captured at the most dangerous band reached this mission. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float PeakRewardMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float EnvironmentalPollutionDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Risk")
	float EnvironmentalPulseIntervalSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionModifierSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Modifier")
	FName ModifierId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Modifier")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Modifier")
	FText Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Modifier")
	FSoftObjectPath IconPath;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionRuleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float StabilityDrainMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float EnemyBudgetMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float EliteHealthMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float EliteAttackMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float PollutionDamageMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float RewardMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float WorldMaterialLootMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Rules")
	float GravityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRewardRuntimeModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	float Multiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	int32 FlatBonusBudget = 0;
};
