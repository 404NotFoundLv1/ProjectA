#include "Core/PRAssetManager.h"

#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Engine/StreamableManager.h"
#include "ProjectA.h"

UPRAssetManager* UPRAssetManager::Get()
{
	if (!UAssetManager::IsInitialized())
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift AssetManager is not initialized."));
		return nullptr;
	}

	UPRAssetManager* Manager = Cast<UPRAssetManager>(&UAssetManager::Get());
	if (!Manager)
	{
		UE_LOG(
			LogProjectA,
			Error,
			TEXT("ProjectRift requires UPRAssetManager but the active manager is %s."),
			*GetNameSafe(&UAssetManager::Get()));
	}

	return Manager;
}

FPrimaryAssetId UPRAssetManager::MakeItemPrimaryAssetId(const FName ItemId)
{
	return ItemId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRItemDataAsset::ItemPrimaryAssetType, ItemId);
}

FPrimaryAssetId UPRAssetManager::MakeLootTablePrimaryAssetId(const FName AssetName)
{
	return AssetName.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRLootTableDataAsset::LootTablePrimaryAssetType, AssetName);
}

FPrimaryAssetId UPRAssetManager::MakeMissionPrimaryAssetId(const FName MissionId)
{
	return MissionId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRMissionProgressionDataAsset::MissionPrimaryAssetType, MissionId);
}

void UPRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	ValidatePrimaryAssetType(UPRItemDataAsset::ItemPrimaryAssetType);
	ValidatePrimaryAssetType(UPRLootTableDataAsset::LootTablePrimaryAssetType);
	ValidatePrimaryAssetType(UPRMissionProgressionDataAsset::MissionPrimaryAssetType);
}

UPRItemDataAsset* UPRAssetManager::GetLoadedItemData(const FName ItemId) const
{
	return Cast<UPRItemDataAsset>(GetPrimaryAssetObject(MakeItemPrimaryAssetId(ItemId)));
}

UPRLootTableDataAsset* UPRAssetManager::GetLoadedLootTable(const FName AssetName) const
{
	return Cast<UPRLootTableDataAsset>(GetPrimaryAssetObject(MakeLootTablePrimaryAssetId(AssetName)));
}

UPRItemDataAsset* UPRAssetManager::LoadItemDataSync(const FName ItemId)
{
	return Cast<UPRItemDataAsset>(LoadPrimaryAssetSync(
		MakeItemPrimaryAssetId(ItemId),
		UPRItemDataAsset::StaticClass()));
}

UPRLootTableDataAsset* UPRAssetManager::LoadLootTableSync(const FName AssetName)
{
	return Cast<UPRLootTableDataAsset>(LoadPrimaryAssetSync(
		MakeLootTablePrimaryAssetId(AssetName),
		UPRLootTableDataAsset::StaticClass()));
}

UPRMissionProgressionDataAsset* UPRAssetManager::LoadMissionSync(const FName MissionId)
{
	return Cast<UPRMissionProgressionDataAsset>(LoadPrimaryAssetSync(
		MakeMissionPrimaryAssetId(MissionId),
		UPRMissionProgressionDataAsset::StaticClass()));
}

TSharedPtr<FStreamableHandle> UPRAssetManager::LoadItemDataAsync(
	const FName ItemId,
	FPRItemDataLoadComplete Completion)
{
	const FPrimaryAssetId AssetId = MakeItemPrimaryAssetId(ItemId);
	if (!AssetId.IsValid())
	{
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	if (UPRItemDataAsset* LoadedItem = GetLoadedItemData(ItemId))
	{
		Completion.ExecuteIfBound(LoadedItem);
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAsset(
		AssetId,
		TArray<FName>(),
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, ItemId, Completion]() mutable
			{
				UPRItemDataAsset* LoadedItem = GetLoadedItemData(ItemId);
				if (!LoadedItem)
				{
					UE_LOG(LogProjectA, Warning, TEXT("Async primary item load completed without a valid object. Id=%s"), *MakeItemPrimaryAssetId(ItemId).ToString());
				}
				Completion.ExecuteIfBound(LoadedItem);
			}));

	if (!Handle)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Async primary item load did not return a handle. Id=%s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
	}

	return Handle;
}

TSharedPtr<FStreamableHandle> UPRAssetManager::LoadLootTableAsync(
	const FName AssetName,
	FPRLootTableLoadComplete Completion)
{
	const FPrimaryAssetId AssetId = MakeLootTablePrimaryAssetId(AssetName);
	if (!AssetId.IsValid())
	{
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	if (UPRLootTableDataAsset* LoadedLootTable = GetLoadedLootTable(AssetName))
	{
		Completion.ExecuteIfBound(LoadedLootTable);
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAsset(
		AssetId,
		TArray<FName>(),
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, AssetName, Completion]() mutable
			{
				UPRLootTableDataAsset* LoadedLootTable = GetLoadedLootTable(AssetName);
				if (!LoadedLootTable)
				{
					UE_LOG(LogProjectA, Warning, TEXT("Async primary loot table load completed without a valid object. Id=%s"), *MakeLootTablePrimaryAssetId(AssetName).ToString());
				}
				Completion.ExecuteIfBound(LoadedLootTable);
			}));

	if (!Handle)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Async primary loot table load did not return a handle. Id=%s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
	}

	return Handle;
}

UObject* UPRAssetManager::LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass)
{
	if (!AssetId.IsValid() || !ExpectedClass)
	{
		return nullptr;
	}

	if (UObject* Existing = GetPrimaryAssetObject(AssetId))
	{
		if (Existing->IsA(ExpectedClass))
		{
			return Existing;
		}

		UE_LOG(
			LogProjectA,
			Warning,
			TEXT("Loaded primary asset has the wrong type. Id=%s Expected=%s Actual=%s"),
			*AssetId.ToString(),
			*GetNameSafe(ExpectedClass),
			*GetNameSafe(Existing->GetClass()));
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		return nullptr;
	}

	UObject* Loaded = AssetPath.TryLoad();
	if (!Loaded || !Loaded->IsA(ExpectedClass))
	{
		UE_LOG(
			LogProjectA,
			Warning,
			TEXT("Primary asset load/type validation failed. Id=%s Path=%s Expected=%s Actual=%s"),
			*AssetId.ToString(),
			*AssetPath.ToString(),
			*GetNameSafe(ExpectedClass),
			*GetNameSafe(Loaded ? Loaded->GetClass() : nullptr));
		return nullptr;
	}

	return Loaded;
}

void UPRAssetManager::ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const
{
	TArray<FPrimaryAssetId> AssetIds;
	GetPrimaryAssetIdList(AssetType, AssetIds);
	if (AssetIds.IsEmpty())
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift primary asset type has no registered assets. Type=%s"), *AssetType.ToString());
		return;
	}

	UE_LOG(LogProjectA, Log, TEXT("ProjectRift primary assets registered. Type=%s Count=%d"), *AssetType.ToString(), AssetIds.Num());
}
