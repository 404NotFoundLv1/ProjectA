#include "Progression/PRObjectiveGraphTypes.h"

#include "Misc/Crc.h"

namespace PRObjectiveGraph
{
bool SetDiagnostic(FString* OutDiagnostic, const FString& Diagnostic)
{
	if (OutDiagnostic)
	{
		*OutDiagnostic = Diagnostic;
	}
	return false;
}

bool ArePrerequisitesMet(const FPRObjectiveNodeDefinition& Node, const TMap<FName, EPRObjectiveNodeState>& States)
{
	if (Node.PrerequisiteNodeIds.IsEmpty())
	{
		return true;
	}
	if (Node.PrerequisitePolicy == EPRObjectivePrerequisitePolicy::All)
	{
		return !Node.PrerequisiteNodeIds.ContainsByPredicate([&States](const FName Id)
		{
			return States.FindRef(Id) != EPRObjectiveNodeState::Completed;
		});
	}
	return Node.PrerequisiteNodeIds.ContainsByPredicate([&States](const FName Id)
	{
		return States.FindRef(Id) == EPRObjectiveNodeState::Completed;
	});
}
}

bool FPRObjectiveNodeDefinition::IsValid(FString* OutDiagnostic) const
{
	if (NodeId.IsNone() || TargetCount <= 0 || !FMath::IsFinite(FailureTimeLimitSeconds) || FailureTimeLimitSeconds < 0.0f || RewardBudgetBonus < 0)
	{
		return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective nodes require valid identifiers, counts, timers, and rewards."));
	}
	if (!bOptional && FailureTimeLimitSeconds > 0.0f)
	{
		return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Only optional objective nodes can have a failure timer."));
	}
	TSet<FName> SeenPrerequisites;
	for (const FName PrerequisiteId : PrerequisiteNodeIds)
	{
		if (PrerequisiteId.IsNone() || PrerequisiteId == NodeId || SeenPrerequisites.Contains(PrerequisiteId))
		{
			return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective prerequisites contain an invalid, self, or duplicate id."));
		}
		SeenPrerequisites.Add(PrerequisiteId);
	}
	return true;
}

bool FPRObjectiveGraphDefinition::IsValid(FString* OutDiagnostic) const
{
	if (GraphVersion <= 0 || Nodes.IsEmpty())
	{
		return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective graph requires a positive version and at least one node."));
	}
	TMap<FName, const FPRObjectiveNodeDefinition*> NodesById;
	bool bHasRequiredNode = false;
	for (const FPRObjectiveNodeDefinition& Node : Nodes)
	{
		if (!Node.IsValid(OutDiagnostic) || NodesById.Contains(Node.NodeId))
		{
			if (OutDiagnostic && OutDiagnostic->IsEmpty())
			{
				*OutDiagnostic = TEXT("Objective graph contains duplicate node ids.");
			}
			return false;
		}
		NodesById.Add(Node.NodeId, &Node);
		bHasRequiredNode |= !Node.bOptional;
	}
	if (!bHasRequiredNode)
	{
		return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective graph requires at least one non-optional node."));
	}
	for (const FPRObjectiveNodeDefinition& Node : Nodes)
	{
		for (const FName PrerequisiteId : Node.PrerequisiteNodeIds)
		{
			const FPRObjectiveNodeDefinition* Prerequisite = NodesById.FindRef(PrerequisiteId);
			if (!Prerequisite)
			{
				return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective graph references a missing prerequisite node."));
			}
			if (!Node.bOptional && Prerequisite->bOptional)
			{
				return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Required objective nodes cannot depend on optional nodes."));
			}
		}
	}
	TMap<FName, uint8> VisitStates;
	TFunction<bool(FName)> Visit = [&NodesById, &VisitStates, OutDiagnostic, &Visit](const FName NodeId)
	{
		const uint8 State = VisitStates.FindRef(NodeId);
		if (State == 1)
		{
			return PRObjectiveGraph::SetDiagnostic(OutDiagnostic, TEXT("Objective graph contains a cycle."));
		}
		if (State == 2)
		{
			return true;
		}
		VisitStates.Add(NodeId, 1);
		for (const FName PrerequisiteId : NodesById.FindRef(NodeId)->PrerequisiteNodeIds)
		{
			if (!Visit(PrerequisiteId))
			{
				return false;
			}
		}
		VisitStates.Add(NodeId, 2);
		return true;
	};
	for (const FPRObjectiveNodeDefinition& Node : Nodes)
	{
		if (!Visit(Node.NodeId))
		{
			return false;
		}
	}
	return true;
}

int32 FPRObjectiveGraphDefinition::ComputeSignature() const
{
	TArray<const FPRObjectiveNodeDefinition*> SortedNodes;
	for (const FPRObjectiveNodeDefinition& Node : Nodes)
	{
		SortedNodes.Add(&Node);
	}
	SortedNodes.Sort([](const FPRObjectiveNodeDefinition& Left, const FPRObjectiveNodeDefinition& Right)
	{
		return Left.NodeId.LexicalLess(Right.NodeId);
	});
	FString Source = FString::FromInt(GraphVersion);
	for (const FPRObjectiveNodeDefinition* Node : SortedNodes)
	{
		TArray<FName> Prerequisites = Node->PrerequisiteNodeIds;
		Prerequisites.Sort(FNameLexicalLess());
		Source += FString::Printf(TEXT("|%s|%d|%d|%d|%d|%d|%s|%d|%.3f|%d"), *Node->NodeId.ToString(), static_cast<int32>(Node->ObjectiveType), static_cast<int32>(Node->PrerequisitePolicy), static_cast<int32>(Node->ActivationMode), Node->bOptional ? 1 : 0, Node->bDrivesEnemySpawning ? 1 : 0, *Node->TargetId.ToString(), Node->TargetCount, Node->FailureTimeLimitSeconds, Node->RewardBudgetBonus);
		for (const FName Prerequisite : Prerequisites)
		{
			Source += TEXT("|") + Prerequisite.ToString();
		}
	}
	const int32 Signature = static_cast<int32>(FCrc::StrCrc32(*Source));
	return Signature == 0 ? 1 : Signature;
}

bool FPRObjectiveGraphSnapshot::IsValid() const
{
	if (GraphVersion <= 0 || GraphSignature == 0 || Nodes.IsEmpty())
	{
		return false;
	}
	TSet<FName> SeenIds;
	for (const FPRObjectiveNodeSnapshot& Node : Nodes)
	{
		if (Node.NodeId.IsNone() || SeenIds.Contains(Node.NodeId) || Node.CurrentCount < 0 || Node.RecoveryAttempts < 0 || !FMath::IsFinite(Node.RemainingTimeSeconds) || Node.RemainingTimeSeconds < 0.0f)
		{
			return false;
		}
		SeenIds.Add(Node.NodeId);
	}
	return true;
}
