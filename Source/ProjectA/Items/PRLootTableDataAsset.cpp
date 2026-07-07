#include "Items/PRLootTableDataAsset.h"

const FPrimaryAssetType UPRLootTableDataAsset::LootTablePrimaryAssetType(TEXT("ProjectRiftLootTable"));

namespace
{
FPRLootTableEntry MakeLootEntry(const FName ItemId, const float Weight, const EPRItemRarity Rarity = EPRItemRarity::Common)
{
	FPRLootTableEntry Entry;
	Entry.Item.ItemId = ItemId;
	Entry.Item.Count = 1;
	Entry.Item.Level = 1;
	Entry.Item.Rarity = Rarity;
	Entry.Item.Durability = 1.0f;
	Entry.Weight = Weight;
	return Entry;
}
}

UPRLootTableDataAsset::UPRLootTableDataAsset()
{
	Entries = MakeDefaultTestEntries();
}

TArray<FPRLootTableEntry> UPRLootTableDataAsset::MakeDefaultTestEntries()
{
	TArray<FPRLootTableEntry> DefaultEntries;
	DefaultEntries.Add(MakeLootEntry(TEXT("HealthInjector"), 40.0f, EPRItemRarity::Common));
	DefaultEntries.Add(MakeLootEntry(TEXT("ShieldPack"), 30.0f, EPRItemRarity::Common));
	DefaultEntries.Add(MakeLootEntry(TEXT("EnergyCrystal"), 20.0f, EPRItemRarity::Uncommon));
	DefaultEntries.Add(MakeLootEntry(TEXT("CommonChip"), 10.0f, EPRItemRarity::Common));
	return DefaultEntries;
}

FPrimaryAssetId UPRLootTableDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(LootTablePrimaryAssetType, GetFName());
}

bool UPRLootTableDataAsset::IsValidLootTable() const
{
	return GetTotalWeight() > 0.0f;
}

float UPRLootTableDataAsset::GetTotalWeight() const
{
	float TotalWeight = 0.0f;
	for (const FPRLootTableEntry& Entry : Entries)
	{
		if (Entry.IsValid())
		{
			TotalWeight += Entry.Weight;
		}
	}

	return TotalWeight;
}

bool UPRLootTableDataAsset::RollLoot(const float Roll, FPRItemInstance& OutItem) const
{
	OutItem = FPRItemInstance();

	const float TotalWeight = GetTotalWeight();
	if (TotalWeight <= 0.0f)
	{
		return false;
	}

	const float ClampedRoll = FMath::Clamp(Roll, 0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	const FPRLootTableEntry* LastValidEntry = nullptr;

	for (const FPRLootTableEntry& Entry : Entries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		LastValidEntry = &Entry;
		AccumulatedWeight += Entry.Weight;
		if (ClampedRoll < AccumulatedWeight)
		{
			OutItem = Entry.Item;
			return true;
		}
	}

	if (LastValidEntry)
	{
		OutItem = LastValidEntry->Item;
		return true;
	}

	return false;
}

bool UPRLootTableDataAsset::RollRandomLoot(FPRItemInstance& OutItem) const
{
	const float TotalWeight = GetTotalWeight();
	if (TotalWeight <= 0.0f)
	{
		OutItem = FPRItemInstance();
		return false;
	}

	return RollLoot(FMath::FRandRange(0.0f, TotalWeight), OutItem);
}
