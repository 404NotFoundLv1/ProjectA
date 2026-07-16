#include "Persistence/PRProfileTypes.h"

namespace
{
constexpr int32 MaxProcessedSettlementIds = 128;
constexpr int32 MaxProcessedRepairTransactionIds = 128;

void NormalizeNames(TArray<FName>& Names)
{
	TSet<FName> Seen;
	Names.RemoveAll([&Seen](const FName Name)
	{
		if (Name.IsNone() || Seen.Contains(Name))
		{
			return true;
		}
		Seen.Add(Name);
		return false;
	});
}

bool AreProfileItemsValid(const TArray<FPRItemInstance>& Items)
{
	return !Items.ContainsByPredicate([](const FPRItemInstance& Item) { return !Item.IsValid(); });
}

bool AreProfileNamesValid(const TArray<FName>& Names)
{
	TSet<FName> Seen;
	for (const FName Name : Names)
	{
		if (Name.IsNone() || Seen.Contains(Name))
		{
			return false;
		}
		Seen.Add(Name);
	}
	return true;
}

void NormalizeRoleModuleEntries(TArray<FPRRoleModuleSlotEntry>& Entries)
{
	TSet<EPRRoleModuleSlot> SeenSlots;
	TSet<FName> SeenModules;
	Entries.RemoveAll([&SeenSlots, &SeenModules](const FPRRoleModuleSlotEntry& Entry)
	{
		if (Entry.Slot == EPRRoleModuleSlot::None || Entry.ModuleId.IsNone()
			|| SeenSlots.Contains(Entry.Slot) || SeenModules.Contains(Entry.ModuleId))
		{
			return true;
		}
		SeenSlots.Add(Entry.Slot);
		SeenModules.Add(Entry.ModuleId);
		return false;
	});
}

bool AreRoleModuleEntriesStructurallyValid(const TArray<FPRRoleModuleSlotEntry>& Entries)
{
	TSet<EPRRoleModuleSlot> SeenSlots;
	TSet<FName> SeenModules;
	for (const FPRRoleModuleSlotEntry& Entry : Entries)
	{
		if (Entry.Slot == EPRRoleModuleSlot::None || Entry.ModuleId.IsNone()
			|| SeenSlots.Contains(Entry.Slot) || SeenModules.Contains(Entry.ModuleId))
		{
			return false;
		}
		SeenSlots.Add(Entry.Slot);
		SeenModules.Add(Entry.ModuleId);
	}
	return true;
}
}

FPRProfileOperationResult FPRProfileOperationResult::MakeSuccess(const FGuid& InProfileId)
{
	FPRProfileOperationResult Result;
	Result.ProfileId = InProfileId;
	return Result;
}

FPRProfileOperationResult FPRProfileOperationResult::MakeFailure(
	const EPRProfileOperationStatus InStatus,
	const FString& InDiagnostic,
	const FGuid& InProfileId)
{
	FPRProfileOperationResult Result;
	Result.Status = InStatus;
	Result.ProfileId = InProfileId;
	Result.Diagnostic = InDiagnostic;
	return Result;
}

void FPRProfileSettingsData::Normalize()
{
	CultureName = CultureName.TrimStartAndEnd();
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.0f, 1.0f);
	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 10.0f);
}

bool FPRProfileSettingsData::IsValid() const
{
	return FMath::IsWithinInclusive(MasterVolume, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(MusicVolume, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(EffectsVolume, 0.0f, 1.0f)
		&& FMath::IsWithinInclusive(MouseSensitivity, 0.1f, 10.0f);
}

int32 FPRProfileSnapshot::GetResourceCount(const FName ResourceId) const
{
	const FPRProfileResourceBalance* Balance = ResourceWallet.FindByPredicate(
		[ResourceId](const FPRProfileResourceBalance& Entry) { return Entry.ResourceId == ResourceId; });
	return Balance ? FMath::Max(0, Balance->Count) : 0;
}

void FPRProfileSnapshot::Normalize()
{
	TMap<FName, int64> MergedResources;
	for (const FPRProfileResourceBalance& Balance : ResourceWallet)
	{
		if (!Balance.ResourceId.IsNone() && Balance.Count > 0)
		{
			MergedResources.FindOrAdd(Balance.ResourceId) += Balance.Count;
		}
	}
	ResourceWallet.Reset(MergedResources.Num());
	for (const TPair<FName, int64>& Pair : MergedResources)
	{
		ResourceWallet.Emplace(Pair.Key, static_cast<int32>(FMath::Min<int64>(Pair.Value, MAX_int32)));
	}
	ResourceWallet.Sort([](const FPRProfileResourceBalance& A, const FPRProfileResourceBalance& B)
	{
		return A.ResourceId.LexicalLess(B.ResourceId);
	});

	BackpackItems.RemoveAll([](const FPRItemInstance& Item) { return !Item.IsValid(); });
	WarehouseItems.RemoveAll([](const FPRItemInstance& Item) { return !Item.IsValid(); });

	TSet<FName> EquipmentSlots;
	Equipment.RemoveAll([&EquipmentSlots](const FPRProfileEquipmentEntry& Entry)
	{
		if (Entry.SlotId.IsNone() || !Entry.Item.IsValid() || EquipmentSlots.Contains(Entry.SlotId))
		{
			return true;
		}
		EquipmentSlots.Add(Entry.SlotId);
		return false;
	});

	NormalizeNames(UnlockedRoleIds);
	NormalizeNames(UnlockedRoleModuleIds);
	if (!SelectedRoleId.IsNone() && !UnlockedRoleIds.Contains(SelectedRoleId))
	{
		UnlockedRoleIds.Add(SelectedRoleId);
	}
	NormalizeRoleModuleEntries(EquippedRoleModules);

	TSet<FName> ModuleIds;
	ShipModules.RemoveAll([&ModuleIds](FPRProfileShipModuleState& Module)
	{
		Module.Level = FMath::Max(0, Module.Level);
		if (Module.ModuleId.IsNone() || ModuleIds.Contains(Module.ModuleId))
		{
			return true;
		}
		ModuleIds.Add(Module.ModuleId);
		return false;
	});

	Settings.Normalize();
	NormalizeNames(Story.UnlockedChapterIds);
	NormalizeNames(Story.CompletedStoryNodeIds);
	if (!Story.CurrentChapterId.IsNone() && !Story.UnlockedChapterIds.Contains(Story.CurrentChapterId))
	{
		Story.UnlockedChapterIds.Add(Story.CurrentChapterId);
	}

	TSet<FGuid> SeenSettlementIds;
	ProcessedSettlementIds.RemoveAll([&SeenSettlementIds](const FGuid& SettlementId)
	{
		if (!SettlementId.IsValid() || SeenSettlementIds.Contains(SettlementId))
		{
			return true;
		}
		SeenSettlementIds.Add(SettlementId);
		return false;
	});
	if (ProcessedSettlementIds.Num() > MaxProcessedSettlementIds)
	{
		ProcessedSettlementIds.RemoveAt(0, ProcessedSettlementIds.Num() - MaxProcessedSettlementIds);
	}

	TSet<FGuid> SeenRepairTransactionIds;
	ProcessedRepairTransactionIds.RemoveAll([&SeenRepairTransactionIds](const FGuid& TransactionId)
	{
		if (!TransactionId.IsValid() || SeenRepairTransactionIds.Contains(TransactionId))
		{
			return true;
		}
		SeenRepairTransactionIds.Add(TransactionId);
		return false;
	});
	if (ProcessedRepairTransactionIds.Num() > MaxProcessedRepairTransactionIds)
	{
		ProcessedRepairTransactionIds.RemoveAt(0, ProcessedRepairTransactionIds.Num() - MaxProcessedRepairTransactionIds);
	}
}

bool FPRProfileSnapshot::IsValid(FString* OutDiagnostic) const
{
	TSet<FName> ResourceIds;
	for (const FPRProfileResourceBalance& Balance : ResourceWallet)
	{
		if (Balance.ResourceId.IsNone() || Balance.Count <= 0 || ResourceIds.Contains(Balance.ResourceId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Resource wallet contains an invalid or duplicate entry."); }
			return false;
		}
		ResourceIds.Add(Balance.ResourceId);
	}
	if (!AreProfileItemsValid(BackpackItems) || !AreProfileItemsValid(WarehouseItems))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Inventory contains an invalid item instance."); }
		return false;
	}
	TSet<FName> EquipmentSlots;
	for (const FPRProfileEquipmentEntry& Entry : Equipment)
	{
		if (Entry.SlotId.IsNone() || !Entry.Item.IsValid() || EquipmentSlots.Contains(Entry.SlotId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Equipment contains an invalid or duplicate slot."); }
			return false;
		}
		EquipmentSlots.Add(Entry.SlotId);
	}
	if (!AreProfileNamesValid(UnlockedRoleIds) || !AreProfileNamesValid(UnlockedRoleModuleIds))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role unlocks contain an invalid or duplicate id."); }
		return false;
	}
	if (!SelectedRoleId.IsNone() && !UnlockedRoleIds.Contains(SelectedRoleId))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Selected role is not unlocked."); }
		return false;
	}
	if (!AreRoleModuleEntriesStructurallyValid(EquippedRoleModules))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Equipped role modules contain an invalid or duplicate slot/module entry."); }
		return false;
	}
	TSet<FName> ModuleIds;
	for (const FPRProfileShipModuleState& Module : ShipModules)
	{
		if (Module.ModuleId.IsNone() || Module.Level < 0 || ModuleIds.Contains(Module.ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Ship modules contain an invalid or duplicate entry."); }
			return false;
		}
		ModuleIds.Add(Module.ModuleId);
	}
	if (!AreProfileNamesValid(Story.UnlockedChapterIds) || !AreProfileNamesValid(Story.CompletedStoryNodeIds))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Story progress contains an invalid or duplicate id."); }
		return false;
	}
	if (!Story.CurrentChapterId.IsNone() && !Story.UnlockedChapterIds.Contains(Story.CurrentChapterId))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Current chapter is not unlocked."); }
		return false;
	}
	if (!Settings.IsValid())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Player settings are outside their supported ranges."); }
		return false;
	}
	if (ProcessedSettlementIds.Num() > MaxProcessedSettlementIds)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Processed settlement ledger exceeds its supported limit."); }
		return false;
	}
	TSet<FGuid> SettlementIds;
	for (const FGuid& SettlementId : ProcessedSettlementIds)
	{
		if (!SettlementId.IsValid() || SettlementIds.Contains(SettlementId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Processed settlement ledger contains an invalid or duplicate id."); }
			return false;
		}
		SettlementIds.Add(SettlementId);
	}
	if (ProcessedRepairTransactionIds.Num() > MaxProcessedRepairTransactionIds)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Processed repair transaction ledger exceeds its supported limit."); }
		return false;
	}
	TSet<FGuid> RepairTransactionIds;
	for (const FGuid& TransactionId : ProcessedRepairTransactionIds)
	{
		if (!TransactionId.IsValid() || RepairTransactionIds.Contains(TransactionId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Processed repair transaction ledger contains an invalid or duplicate id."); }
			return false;
		}
		RepairTransactionIds.Add(TransactionId);
	}
	return true;
}
