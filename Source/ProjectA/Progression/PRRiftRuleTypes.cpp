#include "Progression/PRRiftRuleTypes.h"

bool FPRRiftRiskBandDefinition::IsValid(FString* OutDiagnostic) const
{
	if (!FMath::IsFinite(MinimumStability) || MinimumStability < 0.0f || MinimumStability > 100.0f
		|| !FMath::IsFinite(EnemyBudgetMultiplier) || EnemyBudgetMultiplier <= 0.0f
		|| !FMath::IsFinite(RewardMultiplier) || RewardMultiplier <= 0.0f
		|| !FMath::IsFinite(EnvironmentalPollutionDamage) || EnvironmentalPollutionDamage < 0.0f
		|| !FMath::IsFinite(EnvironmentalPulseIntervalSeconds) || EnvironmentalPulseIntervalSeconds < 0.0f
		|| (EnvironmentalPollutionDamage > 0.0f && EnvironmentalPulseIntervalSeconds <= 0.0f))
	{
		if (OutDiagnostic)
		{
			*OutDiagnostic = TEXT("Rift risk band contains an invalid threshold or multiplier.");
		}
		return false;
	}
	return true;
}
