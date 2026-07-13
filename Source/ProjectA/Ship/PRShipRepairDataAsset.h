#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Ship/PRShipRepairTypes.h"
#include "PRShipRepairDataAsset.generated.h"

UCLASS(BlueprintType)
class PROJECTA_API UPRShipRepairDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ShipRepairPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	FName RepairProjectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	FName ModuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair", meta = (ClampMin = "1"))
	int32 TargetLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	TArray<FPRShipRepairResourceCost> ResourceCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	TArray<FName> RequiredRepairProjectIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	TArray<FName> RequiredCompletedStoryNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	TArray<FName> UnlockedChapterIdsOnCompletion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship Repair")
	FName NextChapterId = NAME_None;

	UFUNCTION(BlueprintPure, Category = "Ship Repair")
	bool IsContractValid() const;

	bool ValidateContract(FString* OutDiagnostic = nullptr) const;
	static bool ValidateCatalog(const TArray<UPRShipRepairDataAsset*>& Contracts, FString* OutDiagnostic = nullptr);
	FPRShipRepairEvaluation EvaluateRepair(
		const FPRProfileSnapshot& Snapshot,
		const TArray<UPRShipRepairDataAsset*>& Catalog) const;
	bool ApplyRepairToSnapshot(
		FPRProfileSnapshot& Snapshot,
		const TArray<UPRShipRepairDataAsset*>& Catalog,
		FString* OutDiagnostic = nullptr) const;
};
