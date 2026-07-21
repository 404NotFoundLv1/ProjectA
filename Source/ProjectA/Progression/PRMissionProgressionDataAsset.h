#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Persistence/PRProfileTypes.h"
#include "Progression/PRObjectiveGraphTypes.h"
#include "Progression/PRMissionContractTypes.h"
#include "PRMissionProgressionDataAsset.generated.h"

class UWorld;
class UPRRewardBudgetDataAsset;

UCLASS(BlueprintType)
class PROJECTA_API UPRMissionProgressionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType MissionPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TSoftObjectPtr<UWorld> MissionMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	FName ChapterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	FName CompletionStoryNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TArray<FName> RequiredCompletedStoryNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TArray<FName> UnlockedChapterIdsOnCompletion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	FName NextChapterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	bool bStarterMission = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TSoftObjectPtr<UPRRewardBudgetDataAsset> RewardBudget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Contract")
	FPRMissionContract Contract;

	/** Optional v0.8.1 graph. Empty preserves legacy single hold-objective maps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission|Objectives")
	FPRObjectiveGraphDefinition ObjectiveGraph;

	bool IsContractValid(FString* OutDiagnostic = nullptr) const;
	bool ValidateMissionContract(FString* OutDiagnostic = nullptr) const;
	bool ValidateObjectiveGraph(FString* OutDiagnostic = nullptr) const;
	bool HasObjectiveGraph() const { return !ObjectiveGraph.Nodes.IsEmpty(); }
	FPRMissionDefinition BuildMissionDefinition(int32 Seed, FString* OutDiagnostic = nullptr) const;
	bool IsEligible(const FPRProfileStoryProgress& Story) const;
	bool ApplyCompletion(FPRProfileStoryProgress& Story) const;
};
