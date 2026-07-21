#include "Core/PRObjectiveGraphComponent.h"

UPRObjectiveGraphComponent::UPRObjectiveGraphComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UPRObjectiveGraphComponent::InitializeGraph(const FPRObjectiveGraphDefinition& InDefinition, FString& OutDiagnostic)
{
	return InitializeGraph(InDefinition, &OutDiagnostic);
}

bool UPRObjectiveGraphComponent::InitializeGraph(const FPRObjectiveGraphDefinition& InDefinition, FString* OutDiagnostic)
{
	if (!InDefinition.IsValid(OutDiagnostic))
	{
		return false;
	}
	Definition = InDefinition;
	RuntimeNodes.Reset();
	for (const FPRObjectiveNodeDefinition& Node : Definition.Nodes)
	{
		RuntimeNodes.Add(Node.NodeId, FRuntimeNode());
	}
	bInitialized = true;
	RefreshAvailableNodes();
	return true;
}

bool UPRObjectiveGraphComponent::RestoreGraph(const FPRObjectiveGraphDefinition& InDefinition, const FPRObjectiveGraphSnapshot& Snapshot, FString* OutDiagnostic)
{
	if (!Snapshot.IsValid() || Snapshot.GraphVersion != InDefinition.GraphVersion || Snapshot.GraphSignature != InDefinition.ComputeSignature())
	{
		if (OutDiagnostic)
		{
			*OutDiagnostic = TEXT("Objective graph snapshot does not match the requested graph.");
		}
		return false;
	}
	if (!InitializeGraph(InDefinition, OutDiagnostic))
	{
		return false;
	}
	for (const FPRObjectiveNodeSnapshot& SnapshotNode : Snapshot.Nodes)
	{
		FRuntimeNode* RuntimeNode = RuntimeNodes.Find(SnapshotNode.NodeId);
		const FPRObjectiveNodeDefinition* NodeDefinition = FindDefinition(SnapshotNode.NodeId);
		if (!RuntimeNode || !NodeDefinition)
		{
			if (OutDiagnostic)
			{
				*OutDiagnostic = TEXT("Objective graph snapshot references an unknown node.");
			}
			return false;
		}
		RuntimeNode->State = SnapshotNode.State;
		RuntimeNode->CurrentCount = FMath::Clamp(SnapshotNode.CurrentCount, 0, NodeDefinition->TargetCount);
		RuntimeNode->RecoveryAttempts = SnapshotNode.RecoveryAttempts;
		RuntimeNode->RemainingTimeSeconds = FMath::Clamp(SnapshotNode.RemainingTimeSeconds, 0.0f, NodeDefinition->FailureTimeLimitSeconds);
	}
	RefreshAvailableNodes();
	return true;
}

EPRObjectiveNodeState UPRObjectiveGraphComponent::GetNodeState(const FName NodeId) const
{
	if (const FRuntimeNode* Node = RuntimeNodes.Find(NodeId))
	{
		return Node->State;
	}
	return EPRObjectiveNodeState::Failed;
}

int32 UPRObjectiveGraphComponent::GetNodeCurrentCount(const FName NodeId) const
{
	if (const FRuntimeNode* Node = RuntimeNodes.Find(NodeId))
	{
		return Node->CurrentCount;
	}
	return 0;
}

bool UPRObjectiveGraphComponent::ActivateNode(const FName NodeId)
{
	if (!bInitialized)
	{
		return false;
	}
	FRuntimeNode* Node = RuntimeNodes.Find(NodeId);
	if (!Node || Node->State != EPRObjectiveNodeState::Available)
	{
		return false;
	}
	Node->State = EPRObjectiveNodeState::Active;
	return true;
}

bool UPRObjectiveGraphComponent::SetNodeCurrentCount(const FName NodeId, const int32 NewCurrentCount)
{
	FRuntimeNode* RuntimeNode = RuntimeNodes.Find(NodeId);
	const FPRObjectiveNodeDefinition* NodeDefinition = FindDefinition(NodeId);
	if (!bInitialized || !RuntimeNode || !NodeDefinition || RuntimeNode->State != EPRObjectiveNodeState::Active || NewCurrentCount < RuntimeNode->CurrentCount)
	{
		return false;
	}
	RuntimeNode->CurrentCount = FMath::Clamp(NewCurrentCount, 0, NodeDefinition->TargetCount);
	if (RuntimeNode->CurrentCount >= NodeDefinition->TargetCount)
	{
		CompleteNode(NodeId);
	}
	return true;
}

bool UPRObjectiveGraphComponent::FailNode(const FName NodeId)
{
	FRuntimeNode* RuntimeNode = RuntimeNodes.Find(NodeId);
	const FPRObjectiveNodeDefinition* NodeDefinition = FindDefinition(NodeId);
	if (!bInitialized || !RuntimeNode || !NodeDefinition || !NodeDefinition->bOptional
		|| (RuntimeNode->State != EPRObjectiveNodeState::Available && RuntimeNode->State != EPRObjectiveNodeState::Active))
	{
		return false;
	}
	RuntimeNode->State = EPRObjectiveNodeState::Failed;
	RuntimeNode->RemainingTimeSeconds = 0.0f;
	return true;
}

bool UPRObjectiveGraphComponent::AdvanceTime(const float DeltaSeconds)
{
	if (!bInitialized || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return false;
	}
	bool bChanged = false;
	for (const FPRObjectiveNodeDefinition& NodeDefinition : Definition.Nodes)
	{
		FRuntimeNode* RuntimeNode = RuntimeNodes.Find(NodeDefinition.NodeId);
		if (!RuntimeNode || !NodeDefinition.bOptional || NodeDefinition.FailureTimeLimitSeconds <= 0.0f
			|| (RuntimeNode->State != EPRObjectiveNodeState::Available && RuntimeNode->State != EPRObjectiveNodeState::Active))
		{
			continue;
		}
		RuntimeNode->RemainingTimeSeconds = FMath::Max(0.0f, RuntimeNode->RemainingTimeSeconds - DeltaSeconds);
		bChanged = true;
		if (RuntimeNode->RemainingTimeSeconds <= 0.0f)
		{
			RuntimeNode->State = EPRObjectiveNodeState::Failed;
		}
	}
	return bChanged;
}

bool UPRObjectiveGraphComponent::SetNodeProgressNormalized(const FName NodeId, const float Progress)
{
	const FPRObjectiveNodeDefinition* NodeDefinition = FindDefinition(NodeId);
	return NodeDefinition && SetNodeCurrentCount(NodeId, FMath::RoundToInt(FMath::Clamp(Progress, 0.0f, 1.0f) * NodeDefinition->TargetCount));
}

const FPRObjectiveNodeDefinition* UPRObjectiveGraphComponent::FindNodeDefinition(const FName NodeId) const
{
	return FindDefinition(NodeId);
}

bool UPRObjectiveGraphComponent::IsGraphCompleted() const
{
	if (!bInitialized)
	{
		return false;
	}
	for (const FPRObjectiveNodeDefinition& Node : Definition.Nodes)
	{
		if (!Node.bOptional && GetNodeState(Node.NodeId) != EPRObjectiveNodeState::Completed)
		{
			return false;
		}
	}
	return true;
}

TArray<FPRObjectiveSummary> UPRObjectiveGraphComponent::GetVisibleSummaries() const
{
	TArray<FPRObjectiveSummary> Result;
	for (const FPRObjectiveNodeDefinition& Node : Definition.Nodes)
	{
		const FRuntimeNode* RuntimeNode = RuntimeNodes.Find(Node.NodeId);
		if (!RuntimeNode || RuntimeNode->State == EPRObjectiveNodeState::Locked)
		{
			continue;
		}
		FPRObjectiveSummary& Summary = Result.AddDefaulted_GetRef();
		Summary.NodeId = Node.NodeId;
		Summary.DisplayName = Node.DisplayName.IsEmpty() ? FText::FromName(Node.NodeId) : Node.DisplayName;
		Summary.ObjectiveType = Node.ObjectiveType;
		Summary.State = RuntimeNode->State;
		Summary.CurrentCount = RuntimeNode->CurrentCount;
		Summary.TargetCount = Node.TargetCount;
		Summary.Progress = Node.TargetCount > 0 ? static_cast<float>(RuntimeNode->CurrentCount) / static_cast<float>(Node.TargetCount) : 0.0f;
		Summary.bOptional = Node.bOptional;
		Summary.RemainingTimeSeconds = RuntimeNode->RemainingTimeSeconds;
		Summary.RewardBudgetBonus = Node.RewardBudgetBonus;
	}
	return Result;
}

FPRObjectiveGraphSnapshot UPRObjectiveGraphComponent::BuildSnapshot() const
{
	FPRObjectiveGraphSnapshot Snapshot;
	if (!bInitialized)
	{
		return Snapshot;
	}
	Snapshot.GraphVersion = Definition.GraphVersion;
	Snapshot.GraphSignature = Definition.ComputeSignature();
	for (const FPRObjectiveNodeDefinition& Node : Definition.Nodes)
	{
		const FRuntimeNode* RuntimeNode = RuntimeNodes.Find(Node.NodeId);
		if (!RuntimeNode)
		{
			continue;
		}
		FPRObjectiveNodeSnapshot& SnapshotNode = Snapshot.Nodes.AddDefaulted_GetRef();
		SnapshotNode.NodeId = Node.NodeId;
		SnapshotNode.State = RuntimeNode->State;
		SnapshotNode.CurrentCount = RuntimeNode->CurrentCount;
		SnapshotNode.RecoveryAttempts = RuntimeNode->RecoveryAttempts;
		SnapshotNode.RemainingTimeSeconds = RuntimeNode->RemainingTimeSeconds;
	}
	return Snapshot;
}

int32 UPRObjectiveGraphComponent::GetCompletedOptionalRewardBudget() const
{
	int32 Total = 0;
	for (const FPRObjectiveNodeDefinition& Node : Definition.Nodes)
	{
		if (Node.bOptional && GetNodeState(Node.NodeId) == EPRObjectiveNodeState::Completed)
		{
			Total += FMath::Max(0, Node.RewardBudgetBonus);
		}
	}
	return Total;
}

bool UPRObjectiveGraphComponent::FailIncompleteOptionalNodes()
{
	bool bChanged = false;
	for (TPair<FName, FRuntimeNode>& Pair : RuntimeNodes)
	{
		const FPRObjectiveNodeDefinition* NodeDefinition = FindDefinition(Pair.Key);
		if (NodeDefinition && NodeDefinition->bOptional
			&& Pair.Value.State != EPRObjectiveNodeState::Completed
			&& Pair.Value.State != EPRObjectiveNodeState::Failed)
		{
			Pair.Value.State = EPRObjectiveNodeState::Failed;
			Pair.Value.RemainingTimeSeconds = 0.0f;
			bChanged = true;
		}
	}
	return bChanged;
}

bool UPRObjectiveGraphComponent::ArePrerequisitesMet(const FPRObjectiveNodeDefinition& Node) const
{
	if (Node.PrerequisiteNodeIds.IsEmpty())
	{
		return true;
	}
	if (Node.PrerequisitePolicy == EPRObjectivePrerequisitePolicy::All)
	{
		return !Node.PrerequisiteNodeIds.ContainsByPredicate([this](const FName PrerequisiteId)
		{
			return GetNodeState(PrerequisiteId) != EPRObjectiveNodeState::Completed;
		});
	}
	return Node.PrerequisiteNodeIds.ContainsByPredicate([this](const FName PrerequisiteId)
	{
		return GetNodeState(PrerequisiteId) == EPRObjectiveNodeState::Completed;
	});
}

void UPRObjectiveGraphComponent::RefreshAvailableNodes()
{
	if (!bInitialized)
	{
		return;
	}
	bool bChanged = false;
	do
	{
		bChanged = false;
		for (const FPRObjectiveNodeDefinition& NodeDefinition : Definition.Nodes)
		{
			FRuntimeNode* RuntimeNode = RuntimeNodes.Find(NodeDefinition.NodeId);
			if (!RuntimeNode || RuntimeNode->State != EPRObjectiveNodeState::Locked || !ArePrerequisitesMet(NodeDefinition))
			{
				continue;
			}
			RuntimeNode->State = NodeDefinition.ActivationMode == EPRObjectiveActivationMode::Automatic
				? EPRObjectiveNodeState::Active
				: EPRObjectiveNodeState::Available;
			RuntimeNode->RemainingTimeSeconds = NodeDefinition.bOptional
				? FMath::Max(0.0f, NodeDefinition.FailureTimeLimitSeconds)
				: 0.0f;
			bChanged = true;
		}
	}
	while (bChanged);
}

void UPRObjectiveGraphComponent::CompleteNode(const FName NodeId)
{
	if (FRuntimeNode* RuntimeNode = RuntimeNodes.Find(NodeId))
	{
		RuntimeNode->State = EPRObjectiveNodeState::Completed;
	}
	RefreshAvailableNodes();
}

const FPRObjectiveNodeDefinition* UPRObjectiveGraphComponent::FindDefinition(const FName NodeId) const
{
	return Definition.Nodes.FindByPredicate([NodeId](const FPRObjectiveNodeDefinition& DefinitionNode)
	{
		return DefinitionNode.NodeId == NodeId;
	});
}
