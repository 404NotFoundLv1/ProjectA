#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/PRRewardTypes.h"
#include "PRRewardBudgetDataAsset.generated.h"

class UPRLootTableDataAsset;

/** Server-owned economy definition for one mission/chapter reward budget. */
UCLASS(BlueprintType)
class PROJECTA_API UPRRewardBudgetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType RewardBudgetPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool IsValidRewardBudget(FString* OutDiagnostic = nullptr) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards")
	TArray<FPRRewardBudgetSourceDefinition> SourceDefinitions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards")
	TSoftObjectPtr<UPRLootTableDataAsset> PersonalEquipmentLootTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	int32 BaseSuccessBudget = 75;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	int32 AdditionalPlayerBudget = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ObjectiveBudget = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "1"))
	int32 EquipmentCost = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "1"))
	int32 CommonChipCost = 25;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "1"))
	int32 RarePityThreshold = 3;
};
