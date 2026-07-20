#include "Persistence/PRProfileSave.h"

#include "Misc/Crc.h"

namespace
{
FString BuildItemIdentitySeed(
	const FGuid& ProfileId,
	const TCHAR* Container,
	const int32 Index,
	const FName SlotId,
	const FPRItemInstance& Item,
	const int32 Salt)
{
	FString AffixSeed;
	for (const FName Affix : Item.Affixes)
	{
		AffixSeed += Affix.ToString();
		AffixSeed += TEXT("|");
	}
	return FString::Printf(
		TEXT("ProjectRift.ItemIdentity.v5|%s|%s|%d|%s|%s|%d|%d|%d|%.9g|%s|%d"),
		*ProfileId.ToString(EGuidFormats::Digits),
		Container,
		Index,
		*SlotId.ToString(),
		*Item.ItemId.ToString(),
		Item.Count,
		Item.Level,
		static_cast<int32>(Item.Rarity),
		Item.Durability,
		*AffixSeed,
		Salt);
}

FGuid MakeDeterministicItemGuid(const FString& Seed)
{
	const uint32 A = FCrc::StrCrc32(*Seed, 0x11A91D5Bu);
	const uint32 B = FCrc::StrCrc32(*Seed, 0x5C7139E1u);
	const uint32 C = FCrc::StrCrc32(*Seed, 0x9E3779B9u);
	uint32 D = FCrc::StrCrc32(*Seed, 0xC2B2AE35u);
	if (A == 0 && B == 0 && C == 0 && D == 0)
	{
		D = 1;
	}
	return FGuid(A, B, C, D);
}

void AssignMigratedItemIdentities(const FGuid& ProfileId, FPRProfileSnapshot& Snapshot)
{
	TSet<FGuid> UsedGuids;
	auto AssignItem = [&ProfileId, &UsedGuids](FPRItemInstance& Item, const TCHAR* Container, const int32 Index, const FName SlotId)
	{
		int32 Salt = 0;
		do
		{
			Item.InstanceGuid = MakeDeterministicItemGuid(BuildItemIdentitySeed(ProfileId, Container, Index, SlotId, Item, Salt));
			++Salt;
		}
		while (UsedGuids.Contains(Item.InstanceGuid));
		UsedGuids.Add(Item.InstanceGuid);
	};

	for (int32 Index = 0; Index < Snapshot.BackpackItems.Num(); ++Index)
	{
		AssignItem(Snapshot.BackpackItems[Index], TEXT("Backpack"), Index, NAME_None);
	}
	for (int32 Index = 0; Index < Snapshot.WarehouseItems.Num(); ++Index)
	{
		AssignItem(Snapshot.WarehouseItems[Index], TEXT("Warehouse"), Index, NAME_None);
	}
	for (int32 Index = 0; Index < Snapshot.Equipment.Num(); ++Index)
	{
		FPRProfileEquipmentEntry& Entry = Snapshot.Equipment[Index];
		AssignItem(Entry.Item, TEXT("Equipment"), Index, Entry.SlotId);
	}
}
}

FPRProfileOperationResult UPRProfileSave::MigrateToLatest()
{
	if (SaveVersion > LatestSaveVersion)
	{
		FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::UnsupportedFutureVersion,
			TEXT("The profile was created by a newer save schema."),
			ProfileId);
		Result.LoadedVersion = SaveVersion;
		return Result;
	}
	if (SaveVersion < InitialSaveVersion)
	{
		return FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::MigrationFailed,
			TEXT("The profile save version is older than the supported migration chain."),
			ProfileId);
	}

	const int32 OriginalVersion = SaveVersion;
	if (SaveVersion == LatestSaveVersion)
	{
		FString IdentityDiagnostic;
		if (!Snapshot.HasValidItemIdentities(&IdentityDiagnostic))
		{
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::MigrationFailed, IdentityDiagnostic, ProfileId);
		}
	}
	while (SaveVersion < LatestSaveVersion)
	{
		switch (SaveVersion)
		{
		case 1:
			Snapshot.Normalize();
			SaveVersion = 2;
			break;
		case 2:
			Snapshot.ProcessedSettlementIds.Reset();
			Snapshot.Normalize();
			SaveVersion = 3;
			break;
		case 3:
			Snapshot.ProcessedRepairTransactionIds.Reset();
			Snapshot.Normalize();
			SaveVersion = 4;
			break;
		case 4:
			Snapshot.Normalize();
			AssignMigratedItemIdentities(ProfileId, Snapshot);
			SaveVersion = 5;
			break;
		case 5:
			// v0.7.2 adds final affix payloads. Existing ID-only affixes remain legacy and receive no fabricated rolls.
			Snapshot.Normalize();
			SaveVersion = 6;
			break;
		case 6:
			// v0.7.3 introduces deterministic personal-reward protection. Legacy profiles start clean.
			Snapshot.LootProtectionStates.Reset();
			Snapshot.Normalize();
			SaveVersion = 7;
			break;
		case 7:
			// v0.7.4 persists only GUID references; older profiles begin with no quickbar bindings.
			Snapshot.QuickSlots.Reset();
			Snapshot.Normalize();
			SaveVersion = 8;
			break;
		case 8:
			// v0.7.5 adds durable crafting transaction replay protection.
			Snapshot.ProcessedCraftingTransactionIds.Reset();
			Snapshot.Normalize();
			SaveVersion = 9;
			break;
		default:
			return FPRProfileOperationResult::MakeFailure(
				EPRProfileOperationStatus::MigrationFailed,
				TEXT("The profile migration chain is incomplete."),
				ProfileId);
		}
	}

	Snapshot.Normalize();
	FString Diagnostic;
	if (!IsValid(&Diagnostic))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::MigrationFailed, Diagnostic, ProfileId);
	}

	FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(ProfileId);
	Result.LoadedVersion = OriginalVersion;
	Result.bMigrated = OriginalVersion != SaveVersion;
	return Result;
}

bool UPRProfileSave::IsValid(FString* OutDiagnostic) const
{
	if (!ProfileId.IsValid())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("ProfileId is invalid."); }
		return false;
	}
	if (SaveVersion < InitialSaveVersion || SaveVersion > LatestSaveVersion)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("SaveVersion is outside the supported range."); }
		return false;
	}
	if (DisplayName.TrimStartAndEnd().IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Profile display name is empty."); }
		return false;
	}
	if (!Snapshot.IsValid(OutDiagnostic))
	{
		return false;
	}
	if (SaveVersion >= 5 && !Snapshot.HasValidItemIdentities(OutDiagnostic))
	{
		return false;
	}
	return true;
}

FPRProfileSummary UPRProfileSave::MakeSummary(const bool bIsActive) const
{
	FPRProfileSummary Summary;
	Summary.ProfileId = ProfileId;
	Summary.DisplayName = DisplayName;
	Summary.SaveVersion = SaveVersion;
	Summary.CreatedUtc = CreatedUtc;
	Summary.LastSavedUtc = LastSavedUtc;
	Summary.bIsActive = bIsActive;
	return Summary;
}
