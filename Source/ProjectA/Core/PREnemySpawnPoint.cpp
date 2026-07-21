#include "Core/PREnemySpawnPoint.h"

APREnemySpawnPoint::APREnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

FTransform APREnemySpawnPoint::GetSpawnTransform() const
{
	return GetActorTransform();
}

bool APREnemySpawnPoint::AllowsObjectiveNode(const FName ObjectiveNodeId) const
{
	return bEncounterSpawnEnabled && (AllowedObjectiveNodeIds.IsEmpty() || AllowedObjectiveNodeIds.Contains(ObjectiveNodeId));
}
