#include "Items/PRRewardBudgetDataAsset.h"

const FPrimaryAssetType UPRRewardBudgetDataAsset::RewardBudgetPrimaryAssetType(TEXT("ProjectRiftRewardBudget"));

FPrimaryAssetId UPRRewardBudgetDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(RewardBudgetPrimaryAssetType, GetFName());
}

bool UPRRewardBudgetDataAsset::IsValidRewardBudget(FString* OutDiagnostic) const
{
	if (PersonalEquipmentLootTable.IsNull())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("PersonalEquipmentLootTable is required."); }
		return false;
	}
	if (BaseSuccessBudget < 0 || AdditionalPlayerBudget < 0 || ObjectiveBudget < 0 || EquipmentCost <= 0 || CommonChipCost <= 0 || RarePityThreshold <= 0)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Reward budget contains an invalid economic value."); }
		return false;
	}
	return true;
}
