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
