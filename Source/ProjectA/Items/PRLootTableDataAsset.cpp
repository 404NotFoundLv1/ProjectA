#include "Items/PRLootTableDataAsset.h"

#include "Core/PRAssetManager.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PREquipmentDataAsset.h"

const FPrimaryAssetType UPRLootTableDataAsset::LootTablePrimaryAssetType(TEXT("ProjectRiftLootTable"));

namespace
{
FPRLootTableEntry MakeLootEntry(const FName ItemId, const float Weight, const EPRItemRarity Rarity = EPRItemRarity::Common, const bool bGenerateEquipmentAffixes = false)
{
	FPRLootTableEntry Entry;
	Entry.Item.ItemId = ItemId;
	Entry.Item.Count = 1;
	Entry.Item.Level = 1;
	Entry.Item.Rarity = Rarity;
	Entry.Item.Durability = 1.0f;
	Entry.Weight = Weight;
	Entry.bGenerateEquipmentAffixes = bGenerateEquipmentAffixes;
	return Entry;
}
}

UPRLootTableDataAsset::UPRLootTableDataAsset()
{
	Entries = MakeDefaultTestEntries();
	EquipmentRarityRules = UPRAffixGenerationLibrary::MakeDefaultRarityRules();
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

bool UPRLootTableDataAsset::RollSeededLoot(const int32 LootSeed, FPRItemInstance& OutItem, FString& OutDiagnostic) const
{
	return RollSeededLootFiltered(LootSeed, NAME_None, EPRItemRarity::Common, OutItem, OutDiagnostic);
}

bool UPRLootTableDataAsset::RollSeededLootFiltered(
	const int32 LootSeed,
	const FName ExcludedItemId,
	const EPRItemRarity MinimumRarity,
	FPRItemInstance& OutItem,
	FString& OutDiagnostic) const
{
	OutItem = FPRItemInstance();
	OutDiagnostic.Reset();
	auto IsCandidate = [ExcludedItemId, MinimumRarity](const FPRLootTableEntry& Entry, const bool bApplyExclusion)
	{
		return Entry.IsValid()
			&& Entry.Item.Rarity >= MinimumRarity
			&& (!bApplyExclusion || ExcludedItemId.IsNone() || Entry.Item.ItemId != ExcludedItemId);
	};
	auto GetCandidateWeight = [this, &IsCandidate](const bool bApplyExclusion)
	{
		float Weight = 0.0f;
		for (const FPRLootTableEntry& Entry : Entries)
		{
			if (IsCandidate(Entry, bApplyExclusion))
			{
				Weight += Entry.Weight;
			}
		}
		return Weight;
	};
	bool bApplyExclusion = !ExcludedItemId.IsNone();
	float TotalWeight = GetCandidateWeight(bApplyExclusion);
	if (TotalWeight <= 0.0f && bApplyExclusion)
	{
		// Repeat protection only excludes an item when another legal candidate exists.
		bApplyExclusion = false;
		TotalWeight = GetCandidateWeight(false);
	}
	if (TotalWeight <= 0.0f)
	{
		OutDiagnostic = TEXT("Loot table has no valid weighted entries after reward constraints.");
		return false;
	}
	FRandomStream Stream(LootSeed);
	const float Roll = Stream.FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;
	const FPRLootTableEntry* SelectedEntry = nullptr;
	for (const FPRLootTableEntry& Entry : Entries)
	{
		if (!IsCandidate(Entry, bApplyExclusion))
		{
			continue;
		}
		SelectedEntry = &Entry;
		AccumulatedWeight += Entry.Weight;
		if (Roll < AccumulatedWeight)
		{
			break;
		}
	}
	if (!SelectedEntry)
	{
		OutDiagnostic = TEXT("Loot table did not select an entry.");
		return false;
	}
	OutItem = SelectedEntry->Item;
	if (!SelectedEntry->bGenerateEquipmentAffixes)
	{
		return true;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPREquipmentDataAsset* Equipment = AssetManager ? Cast<UPREquipmentDataAsset>(AssetManager->LoadItemDataSync(OutItem.ItemId)) : nullptr;
	TArray<UPRAffixDefinitionDataAsset*> AffixCatalog;
	if (!Equipment || !AssetManager || !AssetManager->LoadAffixCatalog(AffixCatalog))
	{
		OutItem = FPRItemInstance();
		OutDiagnostic = TEXT("Seeded equipment loot requires a loaded equipment definition and valid affix catalog.");
		return false;
	}
	FPRAffixGenerationResult Generated = UPRAffixGenerationLibrary::GenerateEquipmentInstance(Equipment, LootSeed, AffixCatalog, EquipmentRarityRules);
	if (!Generated.bSuccess)
	{
		OutItem = FPRItemInstance();
		OutDiagnostic = Generated.Diagnostic;
		return false;
	}
	OutItem = Generated.Item;
	return true;
}
