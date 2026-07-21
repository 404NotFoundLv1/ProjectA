#include "Items/PRLootTableLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/PRAssetManager.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRPickupActor.h"
#include "ProjectA.h"

namespace
{
void ApplyMaterialCountMultiplier(FPRItemInstance& Item, const float Multiplier)
{
	if (!Item.IsValid() || !FMath::IsFinite(Multiplier) || FMath::IsNearlyEqual(Multiplier, 1.0f))
	{
		return;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRItemDataAsset* ItemData = AssetManager ? AssetManager->LoadItemDataSync(Item.ItemId) : nullptr;
	if (!ItemData || ItemData->ItemType != EPRItemType::Material)
	{
		return;
	}
	Item.Count = FMath::Clamp(FMath::RoundToInt(Item.Count * FMath::Clamp(Multiplier, 0.25f, 3.0f)), 1, FMath::Max(1, ItemData->MaxStackCount));
}
}

APRPickupActor* UPRLootTableLibrary::SpawnLootPickupFromTable(
	UObject* WorldContextObject,
	const UPRLootTableDataAsset* LootTable,
	TSubclassOf<APRPickupActor> PickupActorClass,
	const FVector SpawnLocation,
	const FRotator SpawnRotation,
	const float RollOverride,
	const float MaterialCountMultiplier)
{
	if (!WorldContextObject || !LootTable || !LootTable->IsValidLootTable() || LootTable->DistributionPolicy != EPRLootDistributionPolicy::SharedWorld)
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
	ApplyMaterialCountMultiplier(RolledItem, MaterialCountMultiplier);

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

APRPickupActor* UPRLootTableLibrary::SpawnSeededLootPickupFromTable(
	UObject* WorldContextObject,
	const UPRLootTableDataAsset* LootTable,
	TSubclassOf<APRPickupActor> PickupActorClass,
	const FVector SpawnLocation,
	const FRotator SpawnRotation,
	const int32 LootSeed,
	const float MaterialCountMultiplier)
{
	if (!WorldContextObject || !LootTable || !LootTable->IsValidLootTable() || LootTable->DistributionPolicy != EPRLootDistributionPolicy::SharedWorld)
	{
		return nullptr;
	}
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: WorldContextObject->GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	if (!PickupActorClass)
	{
		PickupActorClass = APRPickupActor::StaticClass();
	}
	FPRItemInstance RolledItem;
	FString Diagnostic;
	if (!LootTable->RollSeededLoot(LootSeed, RolledItem, Diagnostic) || !RolledItem.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Seeded loot generation failed. Table=%s Seed=%d Diagnostic=%s"), *GetNameSafe(LootTable), LootSeed, *Diagnostic);
		return nullptr;
	}
	ApplyMaterialCountMultiplier(RolledItem, MaterialCountMultiplier);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APRPickupActor* PickupActor = World->SpawnActor<APRPickupActor>(PickupActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!PickupActor)
	{
		return nullptr;
	}
	PickupActor->SetItemInstance(RolledItem);
	return PickupActor;
}
