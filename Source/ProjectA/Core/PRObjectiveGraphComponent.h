#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/PRObjectiveGraphTypes.h"
#include "PRObjectiveGraphComponent.generated.h"

UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRObjectiveGraphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRObjectiveGraphComponent();

	UFUNCTION(BlueprintCallable, Category = "Rift|ObjectiveGraph")
	bool InitializeGraph(const FPRObjectiveGraphDefinition& InDefinition, FString& OutDiagnostic);

	bool InitializeGraph(const FPRObjectiveGraphDefinition& InDefinition, FString* OutDiagnostic = nullptr);
	bool RestoreGraph(const FPRObjectiveGraphDefinition& InDefinition, const FPRObjectiveGraphSnapshot& Snapshot, FString* OutDiagnostic = nullptr);

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	EPRObjectiveNodeState GetNodeState(FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	int32 GetNodeCurrentCount(FName NodeId) const;

	UFUNCTION(BlueprintCallable, Category = "Rift|ObjectiveGraph")
	bool ActivateNode(FName NodeId);

	UFUNCTION(BlueprintCallable, Category = "Rift|ObjectiveGraph")
	bool SetNodeCurrentCount(FName NodeId, int32 NewCurrentCount);

	UFUNCTION(BlueprintCallable, Category = "Rift|ObjectiveGraph")
	bool FailNode(FName NodeId);

	UFUNCTION(BlueprintCallable, Category = "Rift|ObjectiveGraph")
	bool AdvanceTime(float DeltaSeconds);

	bool SetNodeProgressNormalized(FName NodeId, float Progress);
	const FPRObjectiveNodeDefinition* FindNodeDefinition(FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	bool IsGraphCompleted() const;

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	TArray<FPRObjectiveSummary> GetVisibleSummaries() const;

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	int32 GetCompletedOptionalRewardBudget() const;

	/** Settlement ends optional work without changing required-node success. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|ObjectiveGraph")
	bool FailIncompleteOptionalNodes();

	FPRObjectiveGraphSnapshot BuildSnapshot() const;
	const FPRObjectiveGraphDefinition& GetDefinition() const { return Definition; }

private:
	struct FRuntimeNode
	{
		EPRObjectiveNodeState State = EPRObjectiveNodeState::Locked;
		int32 CurrentCount = 0;
		int32 RecoveryAttempts = 0;
		float RemainingTimeSeconds = 0.0f;
	};

	bool ArePrerequisitesMet(const FPRObjectiveNodeDefinition& Node) const;
	void RefreshAvailableNodes();
	void CompleteNode(FName NodeId);
	const FPRObjectiveNodeDefinition* FindDefinition(FName NodeId) const;

	UPROPERTY(Transient)
	FPRObjectiveGraphDefinition Definition;

	TMap<FName, FRuntimeNode> RuntimeNodes;
	bool bInitialized = false;
};
