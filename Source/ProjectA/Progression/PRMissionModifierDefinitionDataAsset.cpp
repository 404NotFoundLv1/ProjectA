#include "Progression/PRMissionModifierDefinitionDataAsset.h"

const FPrimaryAssetType UPRMissionModifierDefinitionDataAsset::ModifierPrimaryAssetType(TEXT("ProjectRiftModifier"));

FPrimaryAssetId UPRMissionModifierDefinitionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ModifierPrimaryAssetType, ModifierId.IsNone() ? GetFName() : ModifierId);
}

bool UPRMissionModifierDefinitionDataAsset::IsValidModifierDefinition(FString* OutDiagnostic) const
{
	const float Values[] = { StabilityDrainMultiplier, EnemyBudgetMultiplier, EliteHealthMultiplier,
		EliteAttackMultiplier, PollutionDamageMultiplier, RewardMultiplier, WorldMaterialLootMultiplier, GravityScale };
	if (ModifierId.IsNone() || DisplayName.IsEmpty() || Description.IsEmpty()
		|| GrantedRuleTags.IsEmpty() || !FMath::IsFinite(GravityScale) || GravityScale <= 0.0f)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission modifier requires id, text, valid tags, and positive gravity scale."); }
		return false;
	}
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f)
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission modifier multipliers must be finite and positive."); }
			return false;
		}
	}
	return true;
}
