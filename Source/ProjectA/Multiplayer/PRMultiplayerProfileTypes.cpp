#include "Multiplayer/PRMultiplayerProfileTypes.h"

namespace
{
constexpr int32 MaxProjectionDisplayNameLength = 64;
constexpr int32 MaxProjectionBackpackEntries = 256;
constexpr int32 MaxProjectionEquipmentEntries = 64;
constexpr int32 MaxProjectionResourceEntries = 128;
constexpr int32 MaxProjectionStoryEntries = 512;
constexpr int32 MaxProjectionShipModuleEntries = 128;
constexpr int32 MaxProjectionRoleEntries = 128;

bool AreItemsValid(const TArray<FPRItemInstance>& Items)
{
	return !Items.ContainsByPredicate([](const FPRItemInstance& Item) { return !Item.IsValid(); });
}

bool AreResourcesValid(const TArray<FPRProfileResourceBalance>& Resources)
{
	TSet<FName> Seen;
	for (const FPRProfileResourceBalance& Resource : Resources)
	{
		if (Resource.ResourceId.IsNone() || Resource.Count <= 0 || Seen.Contains(Resource.ResourceId))
		{
			return false;
		}
		Seen.Add(Resource.ResourceId);
	}
	return true;
}

bool IsEquipmentValid(const TArray<FPRProfileEquipmentEntry>& Equipment)
{
	TSet<FName> SeenSlots;
	for (const FPRProfileEquipmentEntry& Entry : Equipment)
	{
		if (Entry.SlotId.IsNone() || !Entry.Item.IsValid() || SeenSlots.Contains(Entry.SlotId))
		{
			return false;
		}
		SeenSlots.Add(Entry.SlotId);
	}
	return true;
}

bool AreUniqueValidNames(const TArray<FName>& Names)
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

bool IsStoryValid(const FPRProfileStoryProgress& Story)
{
	return Story.UnlockedChapterIds.Num() <= MaxProjectionStoryEntries
		&& Story.CompletedStoryNodeIds.Num() <= MaxProjectionStoryEntries
		&& AreUniqueValidNames(Story.UnlockedChapterIds)
		&& AreUniqueValidNames(Story.CompletedStoryNodeIds)
		&& (Story.CurrentChapterId.IsNone() || Story.UnlockedChapterIds.Contains(Story.CurrentChapterId));
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

bool AreShipModulesValid(const TArray<FPRProfileShipModuleState>& ShipModules)
{
	TSet<FName> Seen;
	for (const FPRProfileShipModuleState& Module : ShipModules)
	{
		if (Module.ModuleId.IsNone() || Module.Level < 0 || Seen.Contains(Module.ModuleId))
		{
			return false;
		}
		Seen.Add(Module.ModuleId);
	}
	return true;
}
}

bool FPRMultiplayerProfileProjection::IsValid(FString* OutDiagnostic) const
{
	const FString TrimmedDisplayName = DisplayName.TrimStartAndEnd();
	if (!ProfileId.IsValid() || TrimmedDisplayName.IsEmpty() || TrimmedDisplayName.Len() > MaxProjectionDisplayNameLength)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Profile id and display name are required."); }
		return false;
	}
	if (BackpackItems.Num() > MaxProjectionBackpackEntries
		|| Equipment.Num() > MaxProjectionEquipmentEntries
		|| ResourceWallet.Num() > MaxProjectionResourceEntries
		|| ShipModules.Num() > MaxProjectionShipModuleEntries
		|| UnlockedRoleIds.Num() > MaxProjectionRoleEntries
		|| UnlockedRoleModuleIds.Num() > MaxProjectionRoleEntries
		|| EquippedRoleModules.Num() > MaxProjectionRoleEntries
		|| !AreItemsValid(BackpackItems)
		|| !IsEquipmentValid(Equipment)
		|| !AreResourcesValid(ResourceWallet)
		|| !AreShipModulesValid(ShipModules)
		|| !AreUniqueValidNames(UnlockedRoleIds)
		|| !AreUniqueValidNames(UnlockedRoleModuleIds)
		|| !AreRoleModuleEntriesStructurallyValid(EquippedRoleModules)
		|| !IsStoryValid(Story))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Profile projection contains invalid runtime data."); }
		return false;
	}
	return true;
}

bool FPRPlayerSettlementReceipt::IsValid(FString* OutDiagnostic) const
{
	if (!ProfileId.IsValid() || MissionId.IsNone() || !RunId.IsValid() || !SettlementId.IsValid())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Settlement receipt identity is incomplete."); }
		return false;
	}
	if ((Result != EPRRiftMissionResult::Success && Result != EPRRiftMissionResult::Failed)
		|| (bGrantStoryProgress && (Result != EPRRiftMissionResult::Success || !bExtracted)))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Settlement receipt result and story grant are inconsistent."); }
		return false;
	}
	if (SettledBackpackItems.Num() > MaxProjectionBackpackEntries
		|| SettledEquipment.Num() > MaxProjectionEquipmentEntries
		|| SettledResourceWallet.Num() > MaxProjectionResourceEntries
		|| SettledUnlockedRoleIds.Num() > MaxProjectionRoleEntries
		|| SettledUnlockedRoleModuleIds.Num() > MaxProjectionRoleEntries
		|| SettledEquippedRoleModules.Num() > MaxProjectionRoleEntries
		|| !AreItemsValid(SettledBackpackItems)
		|| !IsEquipmentValid(SettledEquipment)
		|| !AreResourcesValid(SettledResourceWallet)
		|| !AreUniqueValidNames(SettledUnlockedRoleIds)
		|| !AreUniqueValidNames(SettledUnlockedRoleModuleIds)
		|| !AreRoleModuleEntriesStructurallyValid(SettledEquippedRoleModules))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Settlement receipt contains invalid runtime data."); }
		return false;
	}
	return true;
}

TArray<FPRItemInstance> FPRMultiplayerSettlementPolicy::BuildNonExtractedInventory(
	const TArray<FPRItemInstance>& BaselineItems,
	const TArray<FPRItemInstance>& RuntimeItems)
{
	TMap<FName, int32> RemainingRuntimeCounts;
	for (const FPRItemInstance& RuntimeItem : RuntimeItems)
	{
		if (RuntimeItem.IsValid())
		{
			RemainingRuntimeCounts.FindOrAdd(RuntimeItem.ItemId) += RuntimeItem.Count;
		}
	}

	TArray<FPRItemInstance> Result;
	for (const FPRItemInstance& BaselineItem : BaselineItems)
	{
		if (!BaselineItem.IsValid())
		{
			continue;
		}
		int32& RemainingCount = RemainingRuntimeCounts.FindOrAdd(BaselineItem.ItemId);
		const int32 RetainedCount = FMath::Min(BaselineItem.Count, FMath::Max(0, RemainingCount));
		if (RetainedCount > 0)
		{
			FPRItemInstance RetainedItem = BaselineItem;
			RetainedItem.Count = RetainedCount;
			Result.Add(MoveTemp(RetainedItem));
			RemainingCount -= RetainedCount;
		}
	}
	return Result;
}

TArray<FPRProfileResourceBalance> FPRMultiplayerSettlementPolicy::BuildNonExtractedResourceWallet(
	const TArray<FPRProfileResourceBalance>& BaselineWallet,
	const TArray<FPRProfileResourceBalance>& RuntimeWallet)
{
	TMap<FName, int32> RuntimeCounts;
	for (const FPRProfileResourceBalance& Resource : RuntimeWallet)
	{
		if (!Resource.ResourceId.IsNone() && Resource.Count > 0)
		{
			RuntimeCounts.FindOrAdd(Resource.ResourceId) += Resource.Count;
		}
	}

	TArray<FPRProfileResourceBalance> Result;
	for (const FPRProfileResourceBalance& BaselineResource : BaselineWallet)
	{
		if (BaselineResource.ResourceId.IsNone() || BaselineResource.Count <= 0)
		{
			continue;
		}
		const int32 RetainedCount = FMath::Min(
			BaselineResource.Count,
			FMath::Max(0, RuntimeCounts.FindRef(BaselineResource.ResourceId)));
		if (RetainedCount > 0)
		{
			Result.Emplace(BaselineResource.ResourceId, RetainedCount);
		}
	}
	return Result;
}

int32 FPRMultiplayerSettlementPolicy::GetItemCount(const TArray<FPRItemInstance>& Items, const FName ItemId)
{
	int32 Total = 0;
	for (const FPRItemInstance& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			Total += FMath::Max(0, Item.Count);
		}
	}
	return Total;
}
