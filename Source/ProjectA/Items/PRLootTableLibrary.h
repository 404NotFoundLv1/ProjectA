#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PRLootTableLibrary.generated.h"

class APRPickupActor;
class UPRLootTableDataAsset;

UCLASS()
class PROJECTA_API UPRLootTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot", meta = (WorldContext = "WorldContextObject"))
	static APRPickupActor* SpawnLootPickupFromTable(
		UObject* WorldContextObject,
		const UPRLootTableDataAsset* LootTable,
		TSubclassOf<APRPickupActor> PickupActorClass,
		FVector SpawnLocation,
		FRotator SpawnRotation,
		float RollOverride = -1.0f);
};
