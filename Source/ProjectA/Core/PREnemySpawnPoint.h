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

	UFUNCTION(BlueprintPure, Category = "Rift|Encounter")
	bool AllowsObjectiveNode(FName ObjectiveNodeId) const;
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") FName GetSpawnGroupId() const { return SpawnGroupId; }
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") float GetEncounterWeight() const { return EncounterWeight; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Encounter") FName SpawnGroupId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Encounter") TArray<FName> AllowedObjectiveNodeIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Encounter", meta = (ClampMin = "0.0")) float EncounterWeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Encounter") bool bEncounterSpawnEnabled = true;
};
