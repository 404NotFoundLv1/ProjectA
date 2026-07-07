#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PREnemySpawnPoint.generated.h"

/**
 * Server-side marker used by rift spawn managers when choosing enemy wave locations.
 */
UCLASS()
class PROJECTA_API APREnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	APREnemySpawnPoint();

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	FTransform GetSpawnTransform() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Spawning")
	TObjectPtr<USceneComponent> SceneRoot;
};
