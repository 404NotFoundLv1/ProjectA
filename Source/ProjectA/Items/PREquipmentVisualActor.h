#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PREquipmentVisualActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Replicated primitive placeholder for non-weapon equipment appearances. */
UCLASS()
class PROJECTA_API APREquipmentVisualActor : public AActor
{
	GENERATED_BODY()

public:
	APREquipmentVisualActor();

private:
	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> VisualMesh;
};

