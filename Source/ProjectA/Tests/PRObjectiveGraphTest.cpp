#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Core/PRObjectiveTypeActors.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Progression/PRObjectiveGraphTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRObjectiveGraphRuntimeTest,
	"ProjectRift.Rift.ObjectiveGraph.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRObjectiveGraphRuntimeTest::RunTest(const FString& Parameters)
{
	FPRObjectiveGraphDefinition Graph;
	Graph.GraphVersion = 1;

	FPRObjectiveNodeDefinition Collect;
	Collect.NodeId = TEXT("Objective.Collect");
	Collect.ObjectiveType = EPRObjectiveType::Collect;
	Collect.TargetCount = 3;
	Collect.ActivationMode = EPRObjectiveActivationMode::Manual;

	FPRObjectiveNodeDefinition Carry;
	Carry.NodeId = TEXT("Objective.Carry");
	Carry.ObjectiveType = EPRObjectiveType::Carry;
	Carry.TargetCount = 1;
	Carry.PrerequisiteNodeIds = { Collect.NodeId };
	Carry.ActivationMode = EPRObjectiveActivationMode::Automatic;

	FPRObjectiveNodeDefinition OptionalDestroy;
	OptionalDestroy.NodeId = TEXT("Objective.Destroy.Optional");
	OptionalDestroy.ObjectiveType = EPRObjectiveType::Destroy;
	OptionalDestroy.TargetCount = 1;
	OptionalDestroy.bOptional = true;
	OptionalDestroy.PrerequisiteNodeIds = { Collect.NodeId };

	Graph.Nodes = { Collect, Carry, OptionalDestroy };
	FString Diagnostic;
	TestTrue(TEXT("Graph with serial and optional nodes validates"), Graph.IsValid(&Diagnostic));

	UPRObjectiveGraphComponent* Runtime = NewObject<UPRObjectiveGraphComponent>(GetTransientPackage());
	TestNotNull(TEXT("Objective graph runtime can be created"), Runtime);
	TestTrue(TEXT("Runtime initializes a valid graph"), Runtime && Runtime->InitializeGraph(Graph, &Diagnostic));
	TestTrue(TEXT("Collect begins available"), Runtime && Runtime->GetNodeState(Collect.NodeId) == EPRObjectiveNodeState::Available);
	TestTrue(TEXT("Carry begins locked"), Runtime && Runtime->GetNodeState(Carry.NodeId) == EPRObjectiveNodeState::Locked);
	TestTrue(TEXT("Manual collect activation succeeds"), Runtime && Runtime->ActivateNode(Collect.NodeId));
	TestTrue(TEXT("Collect reports partial progress"), Runtime && Runtime->SetNodeCurrentCount(Collect.NodeId, 2));
	TestEqual(TEXT("Collect count is retained"), Runtime ? Runtime->GetNodeCurrentCount(Collect.NodeId) : 0, 2);
	TestTrue(TEXT("Collect completes"), Runtime && Runtime->SetNodeCurrentCount(Collect.NodeId, 3));
	TestEqual(TEXT("Collect state is completed"), Runtime ? Runtime->GetNodeState(Collect.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Completed);
	TestEqual(TEXT("Automatic carry activates after its prerequisite"), Runtime ? Runtime->GetNodeState(Carry.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestTrue(TEXT("Optional node does not complete the graph"), Runtime && !Runtime->IsGraphCompleted());
	TestTrue(TEXT("Carry completes"), Runtime && Runtime->SetNodeCurrentCount(Carry.NodeId, 1));
	TestTrue(TEXT("All required nodes complete the graph"), Runtime && Runtime->IsGraphCompleted());

	FPRObjectiveGraphDefinition CyclicGraph = Graph;
	CyclicGraph.Nodes[0].PrerequisiteNodeIds = { Carry.NodeId };
	TestFalse(TEXT("Cycles are rejected"), CyclicGraph.IsValid(&Diagnostic));

	FPRObjectiveGraphSnapshot Snapshot = Runtime ? Runtime->BuildSnapshot() : FPRObjectiveGraphSnapshot();
	UPRObjectiveGraphComponent* RestoredRuntime = NewObject<UPRObjectiveGraphComponent>(GetTransientPackage());
	TestTrue(TEXT("Snapshot restores the initialized graph"), RestoredRuntime && RestoredRuntime->RestoreGraph(Graph, Snapshot, &Diagnostic));
	TestTrue(TEXT("Restored graph remains completed"), RestoredRuntime && RestoredRuntime->IsGraphCompleted());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRObjectiveGraphFiveTypeFlowTest,
	"ProjectRift.Rift.ObjectiveGraph.FiveTypeFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRObjectiveGraphFiveTypeFlowTest::RunTest(const FString& Parameters)
{
	FPRObjectiveGraphDefinition Graph;
	Graph.GraphVersion = 1;

	FPRObjectiveNodeDefinition Collect;
	Collect.NodeId = TEXT("Objective.Collect");
	Collect.ObjectiveType = EPRObjectiveType::Collect;
	Collect.TargetCount = 3;
	Collect.ActivationMode = EPRObjectiveActivationMode::Manual;

	FPRObjectiveNodeDefinition Carry;
	Carry.NodeId = TEXT("Objective.Carry");
	Carry.ObjectiveType = EPRObjectiveType::Carry;
	Carry.TargetCount = 1;
	Carry.PrerequisiteNodeIds = { Collect.NodeId };
	Carry.ActivationMode = EPRObjectiveActivationMode::Automatic;

	FPRObjectiveNodeDefinition Hunt;
	Hunt.NodeId = TEXT("Objective.Hunt");
	Hunt.ObjectiveType = EPRObjectiveType::Hunt;
	Hunt.TargetCount = 5;
	Hunt.PrerequisiteNodeIds = { Carry.NodeId };
	Hunt.ActivationMode = EPRObjectiveActivationMode::Automatic;

	FPRObjectiveNodeDefinition Destroy;
	Destroy.NodeId = TEXT("Objective.Destroy");
	Destroy.ObjectiveType = EPRObjectiveType::Destroy;
	Destroy.TargetCount = 1;
	Destroy.PrerequisiteNodeIds = { Collect.NodeId };
	Destroy.ActivationMode = EPRObjectiveActivationMode::Automatic;

	FPRObjectiveNodeDefinition Hold;
	Hold.NodeId = TEXT("Objective.Hold");
	Hold.ObjectiveType = EPRObjectiveType::Hold;
	Hold.TargetCount = 15;
	Hold.PrerequisiteNodeIds = { Destroy.NodeId };
	Hold.ActivationMode = EPRObjectiveActivationMode::Automatic;

	Graph.Nodes = { Collect, Carry, Hunt, Destroy, Hold };
	FString Diagnostic;
	TestTrue(TEXT("Five-type serial/parallel graph validates"), Graph.IsValid(&Diagnostic));

	UPRObjectiveGraphComponent* Runtime = NewObject<UPRObjectiveGraphComponent>(GetTransientPackage());
	TestTrue(TEXT("Five-type graph initializes"), Runtime && Runtime->InitializeGraph(Graph, &Diagnostic));
	TestEqual(TEXT("Collect begins manually available"), Runtime ? Runtime->GetNodeState(Collect.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Available);
	TestTrue(TEXT("Collect starts"), Runtime && Runtime->ActivateNode(Collect.NodeId));
	TestTrue(TEXT("Collect completes all authored samples"), Runtime && Runtime->SetNodeCurrentCount(Collect.NodeId, 3));
	TestEqual(TEXT("Carry activates after Collect"), Runtime ? Runtime->GetNodeState(Carry.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestEqual(TEXT("Destroy activates in parallel after Collect"), Runtime ? Runtime->GetNodeState(Destroy.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestEqual(TEXT("Hunt remains locked until Carry"), Runtime ? Runtime->GetNodeState(Hunt.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Locked);
	TestEqual(TEXT("Hold remains locked until Destroy"), Runtime ? Runtime->GetNodeState(Hold.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Locked);
	TestTrue(TEXT("Carry completes"), Runtime && Runtime->SetNodeCurrentCount(Carry.NodeId, 1));
	TestEqual(TEXT("Hunt activates after Carry"), Runtime ? Runtime->GetNodeState(Hunt.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestTrue(TEXT("Destroy completes"), Runtime && Runtime->SetNodeCurrentCount(Destroy.NodeId, 1));
	TestEqual(TEXT("Hold activates after Destroy"), Runtime ? Runtime->GetNodeState(Hold.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestEqual(TEXT("Shared tracker has five visible summaries"), Runtime ? Runtime->GetVisibleSummaries().Num() : 0, 5);

	const FPRObjectiveGraphSnapshot Snapshot = Runtime ? Runtime->BuildSnapshot() : FPRObjectiveGraphSnapshot();
	UPRObjectiveGraphComponent* RestoredRuntime = NewObject<UPRObjectiveGraphComponent>(GetTransientPackage());
	TestTrue(TEXT("Five-type snapshot restores"), RestoredRuntime && RestoredRuntime->RestoreGraph(Graph, Snapshot, &Diagnostic));
	TestEqual(TEXT("Restored Carry remains completed"), RestoredRuntime ? RestoredRuntime->GetNodeState(Carry.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Completed);
	TestEqual(TEXT("Restored Hunt remains active"), RestoredRuntime ? RestoredRuntime->GetNodeState(Hunt.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestEqual(TEXT("Restored Hold remains active"), RestoredRuntime ? RestoredRuntime->GetNodeState(Hold.NodeId) : EPRObjectiveNodeState::Failed, EPRObjectiveNodeState::Active);
	TestTrue(TEXT("Hunt completes after recovery"), RestoredRuntime && RestoredRuntime->SetNodeCurrentCount(Hunt.NodeId, 5));
	TestTrue(TEXT("Hold completes after recovery"), RestoredRuntime && RestoredRuntime->SetNodeCurrentCount(Hold.NodeId, 15));
	TestTrue(TEXT("All five required objective types complete the graph"), RestoredRuntime && RestoredRuntime->IsGraphCompleted());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRObjectiveActorGraphBindingTest,
	"ProjectRift.Rift.ObjectiveGraph.ActorBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRObjectiveActorGraphBindingTest::RunTest(const FString& Parameters)
{
	APRRiftObjectiveActor* Objective = NewObject<APRRiftObjectiveActor>(GetTransientPackage());
	TestNotNull(TEXT("Objective actor can be created for graph binding"), Objective);
	if (!Objective)
	{
		return false;
	}

	const FName GraphNodeId(TEXT("Objective.Collect"));
	Objective->SetObjectiveNodeIdForRuntime(GraphNodeId);
	TestEqual(TEXT("Objective exposes its authored graph node id"), Objective->GetObjectiveNodeId(), GraphNodeId);
	TestTrue(TEXT("Graph nodes use graph activation instead of legacy completion"), Objective->UsesObjectiveGraph());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMissionObjectiveGraphContractTest,
	"ProjectRift.Rift.ObjectiveGraph.MissionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMissionObjectiveGraphContractTest::RunTest(const FString& Parameters)
{
	UPRMissionProgressionDataAsset* Mission = NewObject<UPRMissionProgressionDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Mission asset can carry an objective graph"), Mission);
	if (!Mission)
	{
		return false;
	}

	FPRObjectiveNodeDefinition Hold;
	Hold.NodeId = TEXT("Objective.Hold");
	Hold.ObjectiveType = EPRObjectiveType::Hold;
	Hold.TargetCount = 15;
	Mission->ObjectiveGraph.GraphVersion = 1;
	Mission->ObjectiveGraph.Nodes = { Hold };
	TestTrue(TEXT("A valid authored objective graph is accepted by the mission"), Mission->ValidateObjectiveGraph());
	TestTrue(TEXT("A mission reports that it has authored graph content"), Mission->HasObjectiveGraph());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRObjectiveTypeActorContractTest,
	"ProjectRift.Rift.ObjectiveGraph.ObjectiveTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRObjectiveTypeActorContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Collect objective type exists"), APRCollectObjectiveActor::StaticClass());
	TestNotNull(TEXT("Carry objective type exists"), APRCarryObjectiveActor::StaticClass());
	TestNotNull(TEXT("Destroy objective type exists"), APRDestroyObjectiveActor::StaticClass());
	TestNotNull(TEXT("Hunt objective type exists"), APRHuntObjectiveActor::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCarryObjectiveIdentityTest,
	"ProjectRift.Rift.ObjectiveGraph.CarryIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRCarryObjectiveIdentityTest::RunTest(const FString& Parameters)
{
	APRCarryObjectiveActor* Carry = NewObject<APRCarryObjectiveActor>(GetTransientPackage());
	const FGuid CoreGuid = FGuid::NewGuid();
	TestNotNull(TEXT("Carry objective can track a recovered core identity"), Carry);
	TestTrue(TEXT("First valid core identity is accepted"), Carry && Carry->SetTrackedCoreGuid(CoreGuid));
	TestEqual(TEXT("Carry objective preserves the exact core identity"), Carry ? Carry->GetTrackedCoreGuid() : FGuid(), CoreGuid);
	TestFalse(TEXT("A replacement core cannot overwrite the tracked identity"), Carry && Carry->SetTrackedCoreGuid(FGuid::NewGuid()));
	return true;
}

#endif
