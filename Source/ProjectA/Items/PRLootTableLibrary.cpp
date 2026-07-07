#include "Items/PRLootTableLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRPickupActor.h"
#include "ProjectA.h"

APRPickupActor* UPRLootTableLibrary::SpawnLootPickupFromTable(
	UObject* WorldContextObject,
	const UPRLootTableDataAsset* LootTable,
	TSubclassOf<APRPickupActor> PickupActorClass,
	const FVector SpawnLocation,
	const FRotator SpawnRotation,
	const float RollOverride)
{
	if (!WorldContextObject || !LootTable || !LootTable->IsValidLootTable())
	{
		return nullptr;
	}

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Loot table spawn rejected on client world. Table=%s"), *GetNameSafe(LootTable));
		return nullptr;
	}

	if (!PickupActorClass)
	{
		PickupActorClass = APRPickupActor::StaticClass();
	}

	FPRItemInstance RolledItem;
	const bool bRolledLoot = RollOverride >= 0.0f
		? LootTable->RollLoot(RollOverride, RolledItem)
		: LootTable->RollRandomLoot(RolledItem);
	if (!bRolledLoot || !RolledItem.IsValid())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APRPickupActor* PickupActor = World->SpawnActor<APRPickupActor>(PickupActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!PickupActor)
	{
		return nullptr;
	}

	PickupActor->SetItemInstance(RolledItem);

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Loot table spawned pickup. Table=%s Pickup=%s ItemId=%s Count=%d WeightTotal=%.2f"),
		*GetNameSafe(LootTable),
		*GetNameSafe(PickupActor),
		*RolledItem.ItemId.ToString(),
		RolledItem.Count,
		LootTable->GetTotalWeight());

	return PickupActor;
}
