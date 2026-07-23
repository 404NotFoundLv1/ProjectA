#include "Core/PRProductionLevelMarker.h"

#include "Components/SceneComponent.h"

APRProductionLevelMarker::APRProductionLevelMarker()
{
	bReplicates = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}
