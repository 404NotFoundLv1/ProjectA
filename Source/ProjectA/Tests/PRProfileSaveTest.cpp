#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Core/PRRiftGameMode.h"
#include "Engine/GameInstance.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRWeaponDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Persistence/PRProfileRuntimeBridge.h"
#include "Persistence/PRProfileSave.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Persistence/PRSaveStorage.h"
#include "Player/PRPlayerState.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/PRProfileDebugWidget.h"
#include "Blueprint/UserWidget.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
#include "Serialization/MemoryWriter.h"
#include "Weapons/PRWeaponComponent.h"

namespace
{
FPRItemInstance MakeProfileTestItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Level = 2;
	Item.Rarity = EPRItemRarity::Rare;
	Item.Durability = 0.75f;
	Item.Affixes = { TEXT("Affix.ProfileTest") };
	return Item;
}

UPRProfileSave* MakeProfile(const FString& DisplayName)
{
	UPRProfileSave* Profile = NewObject<UPRProfileSave>(GetTransientPackage());
	Profile->ProfileId = FGuid::NewGuid();
	Profile->DisplayName = DisplayName;
	Profile->SaveVersion = UPRProfileSave::LatestSaveVersion;
	Profile->CreatedUtc = FDateTime(2026, 7, 13, 8, 0, 0);
	Profile->LastSavedUtc = Profile->CreatedUtc;
	Profile->Snapshot.ResourceWallet = {
		FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 12 },
		FPRProfileResourceBalance{ TEXT("Alloy"), 4 }
	};
	Profile->Snapshot.BackpackItems = { MakeProfileTestItem(TEXT("HealthInjector"), 2) };
	Profile->Snapshot.WarehouseItems = { MakeProfileTestItem(TEXT("ShieldCell"), 3) };
	Profile->Snapshot.Equipment = {
		FPRProfileEquipmentEntry{ TEXT("Slot.Primary"), MakeProfileTestItem(TEXT("TestBlade"), 1) }
	};
	Profile->Snapshot.UnlockedRoleIds = { TEXT("Ability.Role.Assault") };
	Profile->Snapshot.SelectedRoleId = TEXT("Ability.Role.Assault");
	Profile->Snapshot.ShipModules = {
		FPRProfileShipModuleState{ TEXT("Ship.Module.Engine"), 2, true }
	};
	Profile->Snapshot.Settings.CultureName = TEXT("zh-Hans");
	Profile->Snapshot.Settings.MasterVolume = 0.8f;
	Profile->Snapshot.Story.CurrentChapterId = TEXT("Chapter.Prologue");
	Profile->Snapshot.Story.UnlockedChapterIds = { TEXT("Chapter.Prologue") };
	Profile->Snapshot.Story.CompletedStoryNodeIds = { TEXT("Story.Prologue.Intro") };
	return Profile;
}

FString MakeTestRoot(const TCHAR* TestName)
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("ProfileSave"),
		FString::Printf(TEXT("%s-%s"), TestName, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

bool StructContainsObjectReference(const UStruct* Struct)
{
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* Property = *It;
		if (CastField<FObjectPropertyBase>(Property))
		{
			return true;
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				return true;
			}
			if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				if (StructContainsObjectReference(InnerStruct->Struct))
				{
					return true;
				}
			}
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructContainsObjectReference(StructProperty->Struct))
			{
				return true;
			}
		}
	}
	return false;
}

bool WriteRawEnvelopedSave(USaveGame* SaveGame, const FString& Path)
{
	TArray<uint8> Payload;
	if (!UGameplayStatics::SaveGameToMemory(SaveGame, Payload))
	{
		return false;
	}
	uint32 Magic = 0x56535250;
	uint16 EnvelopeVersion = 1;
	uint32 PayloadSize = Payload.Num();
	uint32 PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	Writer << Magic;
	Writer << EnvelopeVersion;
	Writer << PayloadSize;
	Writer << PayloadCrc;
	Writer.Serialize(Payload.GetData(), Payload.Num());
	return !Writer.IsError() && FFileHelper::SaveArrayToFile(Bytes, *Path);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProfileContractTest,
	"ProjectRift.Save.ProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProfileContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Current player profile schema is version 4"), UPRProfileSave::LatestSaveVersion, 4);
	TestNotNull(
		TEXT("Profile snapshot exposes a durable repair transaction ledger"),
		FindFProperty<FArrayProperty>(FPRProfileSnapshot::StaticStruct(), TEXT("ProcessedRepairTransactionIds")));
	TestNotNull(
		TEXT("Ship repair contracts have a reflected Primary DataAsset class"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRShipRepairDataAsset")));
	TestNotNull(
		TEXT("Ship repair receipts have a reflected pure-data struct"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRShipRepairReceipt")));
	TestFalse(
		TEXT("Profile snapshot recursively contains no UObject references"),
		StructContainsObjectReference(FPRProfileSnapshot::StaticStruct()));

	UPRProfileSave* LegacyProfile = MakeProfile(TEXT("Legacy"));
	LegacyProfile->SaveVersion = 1;
	LegacyProfile->Snapshot.ResourceWallet.Add(FPRProfileResourceBalance{ TEXT("EnergyCrystal"), -3 });
	LegacyProfile->Snapshot.ResourceWallet.Add(FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 5 });
	LegacyProfile->Snapshot.UnlockedRoleIds.Reset();
	LegacyProfile->Snapshot.Settings.MasterVolume = 5.0f;

	FPRProfileOperationResult MigrationResult = LegacyProfile->MigrateToLatest();
	TestTrue(TEXT("v1 profile migrates successfully"), MigrationResult.IsSuccess());
	TestTrue(TEXT("Migration result is marked"), MigrationResult.bMigrated);
	TestEqual(TEXT("Migrated profile reaches v4"), LegacyProfile->SaveVersion, 4);
	TestTrue(TEXT("v1 to v4 migration preserves existing story progress"), LegacyProfile->Snapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.Intro")));
	TestTrue(
		TEXT("Selected role is added to unlocked roles"),
		LegacyProfile->Snapshot.UnlockedRoleIds.Contains(TEXT("Ability.Role.Assault")));
	TestEqual(TEXT("Settings are clamped during migration"), LegacyProfile->Snapshot.Settings.MasterVolume, 1.0f);
	TestEqual(TEXT("Invalid and duplicate wallet rows are normalized"), LegacyProfile->Snapshot.ResourceWallet.Num(), 2);
	TestEqual(TEXT("Duplicate resource balance is merged without the negative row"), LegacyProfile->Snapshot.GetResourceCount(TEXT("EnergyCrystal")), 17);

	UPRProfileSave* FutureProfile = MakeProfile(TEXT("Future"));
	FutureProfile->SaveVersion = UPRProfileSave::LatestSaveVersion + 1;
	const FPRProfileOperationResult FutureResult = FutureProfile->MigrateToLatest();
	TestEqual(
		TEXT("Future profile is rejected explicitly"),
		FutureResult.Status,
		EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestEqual(TEXT("Future profile remains untouched"), FutureProfile->SaveVersion, 5);

	UPRProfileSave* VersionTwoProfile = MakeProfile(TEXT("Version Two"));
	VersionTwoProfile->SaveVersion = 2;
	VersionTwoProfile->Snapshot.ProcessedSettlementIds.Reset();
	const FPRProfileOperationResult V2MigrationResult = VersionTwoProfile->MigrateToLatest();
	TestTrue(TEXT("v2 profile migrates successfully"), V2MigrationResult.IsSuccess());
	TestTrue(TEXT("v2 migration result is marked"), V2MigrationResult.bMigrated);
	TestEqual(TEXT("v2 profile reaches v4"), VersionTwoProfile->SaveVersion, 4);
	TestTrue(TEXT("v2 profile starts with an empty settlement ledger"), VersionTwoProfile->Snapshot.ProcessedSettlementIds.IsEmpty());

	UPRProfileSave* VersionThreeProfile = MakeProfile(TEXT("Version Three"));
	VersionThreeProfile->SaveVersion = 3;
	VersionThreeProfile->Snapshot.ProcessedRepairTransactionIds.Add(FGuid::NewGuid());
	const FPRProfileOperationResult V3MigrationResult = VersionThreeProfile->MigrateToLatest();
	TestTrue(TEXT("v3 profile migrates successfully"), V3MigrationResult.IsSuccess());
	TestTrue(TEXT("v3 migration result is marked"), V3MigrationResult.bMigrated);
	TestEqual(TEXT("v3 profile reaches v4"), VersionThreeProfile->SaveVersion, 4);
	TestTrue(TEXT("v3 profile starts with an empty repair transaction ledger"), VersionThreeProfile->Snapshot.ProcessedRepairTransactionIds.IsEmpty());
	TestTrue(TEXT("v3 to v4 migration preserves story progress"), VersionThreeProfile->Snapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.Intro")));

	FGuid OldestSettlementId;
	for (int32 Index = 0; Index < 132; ++Index)
	{
		const FGuid SettlementId = FGuid::NewGuid();
		if (Index == 0)
		{
			OldestSettlementId = SettlementId;
		}
		VersionTwoProfile->Snapshot.ProcessedSettlementIds.Add(SettlementId);
	}
	const FGuid InvalidSettlementId;
	VersionTwoProfile->Snapshot.ProcessedSettlementIds.Insert(InvalidSettlementId, 0);
	const FGuid DuplicateSettlementId = VersionTwoProfile->Snapshot.ProcessedSettlementIds.Last();
	VersionTwoProfile->Snapshot.ProcessedSettlementIds.Add(DuplicateSettlementId);
	VersionTwoProfile->Snapshot.Normalize();
	TestEqual(TEXT("Settlement ledger is capped at 128 entries"), VersionTwoProfile->Snapshot.ProcessedSettlementIds.Num(), 128);
	TestFalse(TEXT("Settlement ledger trims the oldest processed id first"), VersionTwoProfile->Snapshot.ProcessedSettlementIds.Contains(OldestSettlementId));
	TestFalse(TEXT("Settlement ledger removes invalid ids"), VersionTwoProfile->Snapshot.ProcessedSettlementIds.Contains(InvalidSettlementId));
	TSet<FGuid> UniqueSettlementIds(VersionTwoProfile->Snapshot.ProcessedSettlementIds);
	TestEqual(TEXT("Settlement ledger removes duplicate ids"), UniqueSettlementIds.Num(), VersionTwoProfile->Snapshot.ProcessedSettlementIds.Num());

	FGuid OldestRepairTransactionId;
	for (int32 Index = 0; Index < 132; ++Index)
	{
		const FGuid TransactionId = FGuid::NewGuid();
		if (Index == 0)
		{
			OldestRepairTransactionId = TransactionId;
		}
		VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Add(TransactionId);
	}
	const FGuid InvalidRepairTransactionId;
	VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Insert(InvalidRepairTransactionId, 0);
	const FGuid DuplicateRepairTransactionId = VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Last();
	VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Add(DuplicateRepairTransactionId);
	VersionTwoProfile->Snapshot.Normalize();
	TestEqual(TEXT("Repair transaction ledger is capped at 128 entries"), VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Num(), 128);
	TestFalse(TEXT("Repair transaction ledger trims the oldest processed id first"), VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Contains(OldestRepairTransactionId));
	TestFalse(TEXT("Repair transaction ledger removes invalid ids"), VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Contains(InvalidRepairTransactionId));
	TSet<FGuid> UniqueRepairTransactionIds(VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds);
	TestEqual(TEXT("Repair transaction ledger removes duplicate ids"), UniqueRepairTransactionIds.Num(), VersionTwoProfile->Snapshot.ProcessedRepairTransactionIds.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRSafeProfileStorageTest,
	"ProjectRift.Save.SafeStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSafeProfileStorageTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeTestRoot(TEXT("SafeStorage"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	FPRSafeSaveStore Store(TestRoot);

	UPRProfileSave* Profile = MakeProfile(TEXT("First"));
	const FGuid ProfileId = Profile->ProfileId;
	TestTrue(TEXT("First profile write succeeds"), Store.SaveProfile(Profile).IsSuccess());

	Profile->DisplayName = TEXT("Second");
	Profile->Snapshot.ResourceWallet[0].Count = 99;
	TestTrue(TEXT("Second profile write succeeds"), Store.SaveProfile(Profile).IsSuccess());
	TestTrue(TEXT("Primary file exists"), IFileManager::Get().FileExists(*Store.GetProfilePrimaryPath(ProfileId)));
	TestTrue(TEXT("Previous primary is retained as backup"), IFileManager::Get().FileExists(*Store.GetProfileBackupPath(ProfileId)));

	TArray<uint8> PrimaryBytes;
	TestTrue(TEXT("Can read primary for corruption test"), FFileHelper::LoadFileToArray(PrimaryBytes, *Store.GetProfilePrimaryPath(ProfileId)));
	if (!PrimaryBytes.IsEmpty())
	{
		PrimaryBytes.SetNum(FMath::Max(1, PrimaryBytes.Num() / 2));
		TestTrue(TEXT("Can truncate primary"), FFileHelper::SaveArrayToFile(PrimaryBytes, *Store.GetProfilePrimaryPath(ProfileId)));
	}

	UPRProfileSave* RecoveredProfile = nullptr;
	const FPRProfileOperationResult RecoveryResult = Store.LoadProfile(ProfileId, RecoveredProfile);
	TestTrue(TEXT("Backup recovery succeeds"), RecoveryResult.IsSuccess());
	TestTrue(TEXT("Backup recovery is reported"), RecoveryResult.bRecoveredFromBackup);
	TestNotNull(TEXT("Recovered profile is returned"), RecoveredProfile);
	TestEqual(TEXT("Recovered profile contains previous save"), RecoveredProfile ? RecoveredProfile->DisplayName : FString(), FString(TEXT("First")));
	TestTrue(TEXT("One corrupt file is retained"), IFileManager::Get().FileExists(*Store.GetProfileCorruptPath(ProfileId)));

	UPRProfileSave* ReloadedProfile = nullptr;
	const FPRProfileOperationResult ReloadResult = Store.LoadProfile(ProfileId, ReloadedProfile);
	TestTrue(TEXT("Repaired primary reloads"), ReloadResult.IsSuccess());
	TestFalse(TEXT("Repaired primary no longer needs backup recovery"), ReloadResult.bRecoveredFromBackup);
	TestEqual(TEXT("Repaired primary contains recovered data"), ReloadedProfile ? ReloadedProfile->DisplayName : FString(), FString(TEXT("First")));

	TestTrue(TEXT("Can create a stale temporary file"), FFileHelper::SaveStringToFile(TEXT("stale"), *Store.GetProfileTempPath(ProfileId)));
	UPRProfileSave* LoadedWithStaleTemp = nullptr;
	TestTrue(TEXT("Valid primary loads despite stale temporary data"), Store.LoadProfile(ProfileId, LoadedWithStaleTemp).IsSuccess());
	TestFalse(TEXT("Stale temporary file is cleaned after a valid load"), IFileManager::Get().FileExists(*Store.GetProfileTempPath(ProfileId)));

	Profile->DisplayName = TEXT("Current for semantic fallback");
	TestTrue(TEXT("Can restore a valid current primary"), Store.SaveProfile(Profile).IsSuccess());
	UPRProfileSave* WrongProfile = MakeProfile(TEXT("Wrong ProfileId"));
	TestTrue(
		TEXT("Can inject a valid envelope with the wrong ProfileId"),
		WriteRawEnvelopedSave(WrongProfile, Store.GetProfilePrimaryPath(ProfileId)));
	UPRProfileSave* SemanticRecoveryProfile = nullptr;
	const FPRProfileOperationResult SemanticRecoveryResult = Store.LoadProfile(ProfileId, SemanticRecoveryProfile);
	TestTrue(TEXT("Wrong ProfileId primary falls back to backup"), SemanticRecoveryResult.IsSuccess());
	TestTrue(TEXT("Semantic corruption recovery is reported"), SemanticRecoveryResult.bRecoveredFromBackup);
	TestEqual(TEXT("Semantic recovery returns the requested profile"), SemanticRecoveryProfile ? SemanticRecoveryProfile->ProfileId : FGuid(), ProfileId);

	Profile->DisplayName = TEXT("Backup must not replace future primary");
	TestTrue(TEXT("Can prepare a valid backup before future-version test"), Store.SaveProfile(Profile).IsSuccess());
	UPRProfileSave* FutureOnDisk = MakeProfile(TEXT("Future Primary"));
	FutureOnDisk->ProfileId = ProfileId;
	FutureOnDisk->SaveVersion = UPRProfileSave::LatestSaveVersion + 1;
	TestTrue(TEXT("Can write a syntactically valid future-version envelope"), WriteRawEnvelopedSave(FutureOnDisk, Store.GetProfilePrimaryPath(ProfileId)));
	TArray<uint8> FuturePrimaryBeforeSaveAttempt;
	TArray<uint8> FutureBackupBeforeSaveAttempt;
	FFileHelper::LoadFileToArray(FuturePrimaryBeforeSaveAttempt, *Store.GetProfilePrimaryPath(ProfileId));
	FFileHelper::LoadFileToArray(FutureBackupBeforeSaveAttempt, *Store.GetProfileBackupPath(ProfileId));
	UPRProfileSave* RejectedFutureProfile = nullptr;
	const FPRProfileOperationResult FutureLoadResult = Store.LoadProfile(ProfileId, RejectedFutureProfile);
	TestEqual(TEXT("Future primary is rejected without fallback"), FutureLoadResult.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestFalse(TEXT("Future primary does not report backup recovery"), FutureLoadResult.bRecoveredFromBackup);
	TestNull(TEXT("Future profile is not exposed to callers"), RejectedFutureProfile);
	const FPRProfileOperationResult SaveAgainstFutureResult = Store.SaveProfile(Profile);
	TestEqual(TEXT("Current writer refuses to replace a future primary"), SaveAgainstFutureResult.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	const FDateTime TimestampBeforeSecondRefusedSave = Profile->LastSavedUtc;
	Store.SaveProfile(Profile);
	TestEqual(TEXT("Refused save leaves in-memory LastSavedUtc unchanged"), Profile->LastSavedUtc, TimestampBeforeSecondRefusedSave);
	TArray<uint8> FuturePrimaryAfterSaveAttempt;
	TArray<uint8> FutureBackupAfterSaveAttempt;
	FFileHelper::LoadFileToArray(FuturePrimaryAfterSaveAttempt, *Store.GetProfilePrimaryPath(ProfileId));
	FFileHelper::LoadFileToArray(FutureBackupAfterSaveAttempt, *Store.GetProfileBackupPath(ProfileId));
	TestTrue(TEXT("Refused save leaves future primary byte-identical"), FuturePrimaryAfterSaveAttempt == FuturePrimaryBeforeSaveAttempt);
	TestTrue(TEXT("Refused save leaves recovery backup byte-identical"), FutureBackupAfterSaveAttempt == FutureBackupBeforeSaveAttempt);

	UPRProfileSave* LegacyOnDisk = MakeProfile(TEXT("Legacy Disk Profile"));
	LegacyOnDisk->SaveVersion = 1;
	TestTrue(TEXT("Can save a valid v1 sample"), Store.SaveProfile(LegacyOnDisk).IsSuccess());
	UPRProfileSave* LoadedLegacy = nullptr;
	TestTrue(TEXT("Can load a v1 sample before migration"), Store.LoadProfile(LegacyOnDisk->ProfileId, LoadedLegacy).IsSuccess());
	const FPRProfileOperationResult DiskMigrationResult = LoadedLegacy->MigrateToLatest();
	TestTrue(TEXT("Loaded v1 sample migrates"), DiskMigrationResult.IsSuccess());
	TestTrue(TEXT("Loaded v1 sample reports migration"), DiskMigrationResult.bMigrated);
	TestTrue(TEXT("Migrated v3 sample is safely written"), Store.SaveProfile(LoadedLegacy).IsSuccess());
	UPRProfileSave* ReloadedMigratedProfile = nullptr;
	TestTrue(TEXT("Migrated sample reloads"), Store.LoadProfile(LegacyOnDisk->ProfileId, ReloadedMigratedProfile).IsSuccess());
	TestEqual(TEXT("Migrated sample persists at the latest version"), ReloadedMigratedProfile ? ReloadedMigratedProfile->SaveVersion : 0, UPRProfileSave::LatestSaveVersion);
	TestTrue(TEXT("Migrated sample retains its v1 backup"), IFileManager::Get().FileExists(*Store.GetProfileBackupPath(LegacyOnDisk->ProfileId)));

	UPRProfileSave* DoubleCorruptProfile = MakeProfile(TEXT("Double Corrupt"));
	TestTrue(TEXT("First double-corrupt setup save succeeds"), Store.SaveProfile(DoubleCorruptProfile).IsSuccess());
	DoubleCorruptProfile->DisplayName = TEXT("Double Corrupt Second");
	TestTrue(TEXT("Second double-corrupt setup save succeeds"), Store.SaveProfile(DoubleCorruptProfile).IsSuccess());
	for (const FString& Path : { Store.GetProfilePrimaryPath(DoubleCorruptProfile->ProfileId), Store.GetProfileBackupPath(DoubleCorruptProfile->ProfileId) })
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path);
		Bytes.SetNum(FMath::Max(1, Bytes.Num() / 3));
		FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
	UPRProfileSave* DoubleCorruptLoad = nullptr;
	const FPRProfileOperationResult DoubleCorruptResult = Store.LoadProfile(DoubleCorruptProfile->ProfileId, DoubleCorruptLoad);
	TestEqual(TEXT("Two corrupt copies fail closed"), DoubleCorruptResult.Status, EPRProfileOperationStatus::Corrupt);
	TestNull(TEXT("Two corrupt copies expose no data"), DoubleCorruptLoad);

	const TArray<FGuid> DiscoveredIds = Store.DiscoverProfileIds();
	TestTrue(TEXT("Profile discovery finds the original profile"), DiscoveredIds.Contains(ProfileId));
	TestTrue(TEXT("Profile discovery finds the migrated profile"), DiscoveredIds.Contains(LegacyOnDisk->ProfileId));
	TestTrue(TEXT("Profile discovery keeps GUID directories isolated"), DiscoveredIds.Contains(DoubleCorruptProfile->ProfileId));

	UPRProfileSave* SemanticWriteProfile = MakeProfile(TEXT("Semantic Write"));
	TestTrue(TEXT("Semantic write first save succeeds"), Store.SaveProfile(SemanticWriteProfile).IsSuccess());
	SemanticWriteProfile->DisplayName = TEXT("Semantic Write Second");
	TestTrue(TEXT("Semantic write second save succeeds"), Store.SaveProfile(SemanticWriteProfile).IsSuccess());
	UPRProfileSave* SemanticWrongPrimary = MakeProfile(TEXT("Wrong semantic primary"));
	TestTrue(TEXT("Can inject wrong semantic primary before save"), WriteRawEnvelopedSave(SemanticWrongPrimary, Store.GetProfilePrimaryPath(SemanticWriteProfile->ProfileId)));
	TArray<uint8> KnownGoodBackupBeforeSemanticSave;
	FFileHelper::LoadFileToArray(KnownGoodBackupBeforeSemanticSave, *Store.GetProfileBackupPath(SemanticWriteProfile->ProfileId));
	SemanticWriteProfile->DisplayName = TEXT("Semantic Write Repaired");
	TestTrue(TEXT("Save succeeds while quarantining wrong semantic primary"), Store.SaveProfile(SemanticWriteProfile).IsSuccess());
	TArray<uint8> KnownGoodBackupAfterSemanticSave;
	FFileHelper::LoadFileToArray(KnownGoodBackupAfterSemanticSave, *Store.GetProfileBackupPath(SemanticWriteProfile->ProfileId));
	TestTrue(TEXT("Semantic corruption never replaces known-good backup"), KnownGoodBackupAfterSemanticSave == KnownGoodBackupBeforeSemanticSave);
	TestTrue(TEXT("Wrong semantic primary is retained as corrupt"), IFileManager::Get().FileExists(*Store.GetProfileCorruptPath(SemanticWriteProfile->ProfileId)));

	UPRProfileSave* InvalidContentProfile = MakeProfile(TEXT("Invalid Content"));
	TestTrue(TEXT("Invalid-content first save succeeds"), Store.SaveProfile(InvalidContentProfile).IsSuccess());
	InvalidContentProfile->DisplayName = TEXT("Invalid Content Backup Source");
	TestTrue(TEXT("Invalid-content second save succeeds"), Store.SaveProfile(InvalidContentProfile).IsSuccess());
	UPRProfileSave* InvalidCurrentV2 = MakeProfile(TEXT("Temporary"));
	InvalidCurrentV2->ProfileId = InvalidContentProfile->ProfileId;
	InvalidCurrentV2->DisplayName.Reset();
	TestTrue(TEXT("Can inject current v2 with invalid content"), WriteRawEnvelopedSave(InvalidCurrentV2, Store.GetProfilePrimaryPath(InvalidContentProfile->ProfileId)));
	UPRProfileSave* InvalidContentRecovery = nullptr;
	const FPRProfileOperationResult InvalidContentResult = Store.LoadProfile(InvalidContentProfile->ProfileId, InvalidContentRecovery);
	TestTrue(TEXT("Invalid current v2 content falls back to backup"), InvalidContentResult.IsSuccess());
	TestTrue(TEXT("Invalid current v2 recovery is reported"), InvalidContentResult.bRecoveredFromBackup);
	TestEqual(TEXT("Invalid content recovery returns prior valid profile"), InvalidContentRecovery ? InvalidContentRecovery->DisplayName : FString(), FString(TEXT("Invalid Content")));

	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProfileRuntimeBridgeTest,
	"ProjectRift.Save.RuntimeBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProfileRuntimeBridgeTest::RunTest(const FString& Parameters)
{
	APRPlayerState* SourcePlayerState = NewObject<APRPlayerState>(GetTransientPackage());
	TestNotNull(TEXT("Source player state exists"), SourcePlayerState);
	if (!SourcePlayerState)
	{
		return false;
	}

	SourcePlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TStrongObjectPtr<UPRWeaponDataAsset> WeaponData{ NewObject<UPRWeaponDataAsset>(GetTransientPackage()) };
	WeaponData->ItemId = TEXT("TestRifle");
	WeaponData->AmmoItemId = TEXT("RifleAmmo");
	WeaponData->MagazineCapacity = 12;
	WeaponData->InitialReserveAmmo = 48;
	TStrongObjectPtr<UPRItemDataAsset> AmmoData{ NewObject<UPRItemDataAsset>(GetTransientPackage()) };
	AmmoData->ItemId = TEXT("RifleAmmo");
	AmmoData->ItemType = EPRItemType::Ammunition;
	AmmoData->MaxStackCount = 120;
	SourcePlayerState->GetInventoryComponent()->SetItemDataAssets({ WeaponData.Get(), AmmoData.Get() });
	SourcePlayerState->AddShipResource(TEXT("EnergyCrystal"), 7);
	TestTrue(
		TEXT("Source inventory accepts a profile item"),
		SourcePlayerState->GetInventoryComponent()->AddItem(MakeProfileTestItem(TEXT("HealthInjector"), 2)));
	FString Diagnostic;
	TestTrue(TEXT("Source receives equipped starter rifle"), SourcePlayerState->GetWeaponComponent()->EnsureStarterWeapon(TEXT("TestRifle"), Diagnostic));

	FPRProfileSnapshot Snapshot;
	Snapshot.Equipment.Emplace(TEXT("Slot.Utility"), MakeProfileTestItem(TEXT("LegacyScanner"), 1));
	TestTrue(
		TEXT("Runtime bridge captures current player data"),
		FPRProfileRuntimeBridge::CaptureFromPlayerState(SourcePlayerState, Snapshot, Diagnostic));
	TestEqual(TEXT("Captured selected role"), Snapshot.SelectedRoleId, FName(TEXT("Ability.Role.Assault")));
	TestEqual(TEXT("Captured resource wallet"), Snapshot.GetResourceCount(TEXT("EnergyCrystal")), 7);
	TestEqual(TEXT("Captured backpack includes item and normalized ammo"), Snapshot.BackpackItems.Num(), 2);
	TestEqual(TEXT("Capture stores total rifle ammo rather than partial runtime pools"),
		FPRMultiplayerSettlementPolicy::GetItemCount(Snapshot.BackpackItems, TEXT("RifleAmmo")), 60);
	TestTrue(TEXT("Capture writes Slot.Primary"), Snapshot.Equipment.ContainsByPredicate([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == TEXT("Slot.Primary") && Entry.Item.ItemId == TEXT("TestRifle");
	}));
	TestTrue(TEXT("Capture preserves unknown legacy equipment"), Snapshot.Equipment.ContainsByPredicate([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == TEXT("Slot.Utility") && Entry.Item.ItemId == TEXT("LegacyScanner");
	}));

	APRPlayerState* TargetPlayerState = NewObject<APRPlayerState>(GetTransientPackage());
	TargetPlayerState->GetInventoryComponent()->SetItemDataAssets({ WeaponData.Get(), AmmoData.Get() });
	TestTrue(
		TEXT("Runtime bridge applies the snapshot atomically"),
		FPRProfileRuntimeBridge::ApplyToPlayerState(Snapshot, TargetPlayerState, Diagnostic));
	TestEqual(TEXT("Applied selected role"), TargetPlayerState->GetSelectedRoleModule(), FName(TEXT("Ability.Role.Assault")));
	TestEqual(TEXT("Applied resource wallet"), TargetPlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 7);
	TestEqual(TEXT("Applied backpack keeps ordinary item and reserve ammo"), TargetPlayerState->GetInventoryComponent()->GetInventoryItems().Num(), 2);
	TestEqual(TEXT("Applied profile repartitions full magazine"), TargetPlayerState->GetWeaponComponent()->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Applied profile leaves 48 reserve"), TargetPlayerState->GetWeaponComponent()->GetReserveAmmo(), 48);
	TestEqual(TEXT("Applied profile equips primary rifle"), TargetPlayerState->GetWeaponComponent()->GetEquippedWeapon().ItemId, FName(TEXT("TestRifle")));
	TestTrue(TEXT("Applied profile retains unknown legacy equipment"), TargetPlayerState->GetWeaponComponent()->GetEquipmentEntries().ContainsByPredicate([](const FPRProfileEquipmentEntry& Entry)
	{
		return Entry.SlotId == TEXT("Slot.Utility") && Entry.Item.ItemId == TEXT("LegacyScanner");
	}));

	APRPlayerState* EmptyProfilePlayerState = NewObject<APRPlayerState>(GetTransientPackage());
	EmptyProfilePlayerState->GetInventoryComponent()->SetItemDataAssets({ WeaponData.Get(), AmmoData.Get() });
	FPRProfileSnapshot EmptySnapshot;
	TestTrue(
		TEXT("Applying an empty v4 profile grants the starter weapon atomically"),
		FPRProfileRuntimeBridge::ApplyToPlayerState(EmptySnapshot, EmptyProfilePlayerState, Diagnostic));
	TestEqual(TEXT("Empty profile equips the starter rifle"), EmptyProfilePlayerState->GetWeaponComponent()->GetEquippedWeapon().ItemId, FName(TEXT("TestRifle")));
	TestEqual(TEXT("Empty profile begins with a full magazine"), EmptyProfilePlayerState->GetWeaponComponent()->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Empty profile begins with 48 reserve"), EmptyProfilePlayerState->GetWeaponComponent()->GetReserveAmmo(), 48);

	FPRProfileSnapshot InvalidSnapshot = Snapshot;
	InvalidSnapshot.BackpackItems[0].Count = -1;
	const TArray<FPRItemInstance> ItemsBeforeRejectedApply = TargetPlayerState->GetInventoryComponent()->GetInventoryItems();
	TestFalse(
		TEXT("Runtime bridge rejects an invalid snapshot"),
		FPRProfileRuntimeBridge::ApplyToPlayerState(InvalidSnapshot, TargetPlayerState, Diagnostic));
	TestEqual(
		TEXT("Rejected apply leaves the target backpack unchanged"),
		TargetPlayerState->GetInventoryComponent()->GetInventoryItems().Num(),
		ItemsBeforeRejectedApply.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProfileSubsystemContractTest,
	"ProjectRift.Save.SubsystemContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProfileSubsystemContractTest::RunTest(const FString& Parameters)
{
	const UClass* SubsystemClass = UPRSaveSubsystem::StaticClass();
	TestTrue(
		TEXT("Save subsystem is a GameInstance subsystem"),
		SubsystemClass->IsChildOf(UGameInstanceSubsystem::StaticClass()));
	for (const FName FunctionName : {
		FName(TEXT("CreateProfile")),
		FName(TEXT("GetProfiles")),
		FName(TEXT("ActivateAndLoadProfile")),
		FName(TEXT("DeleteProfile")),
		FName(TEXT("SaveActiveProfile")),
		FName(TEXT("GetActiveProfileSnapshot")),
		FName(TEXT("ReplaceActiveProfileSnapshot")),
		FName(TEXT("ApplyActiveProfileToPlayerState")),
		FName(TEXT("CaptureAndSaveAtSafeCheckpoint")),
		FName(TEXT("CreateLegacyV1ProfileForDevelopment")),
		FName(TEXT("CorruptActivePrimaryForDevelopment")) })
	{
		TestNotNull(*FString::Printf(TEXT("%s is exposed"), *FunctionName.ToString()), SubsystemClass->FindFunctionByName(FunctionName));
	}
	TestTrue(
		TEXT("Development profile panel is a native UserWidget"),
		UPRProfileDebugWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass()));

	const FString TestRoot = MakeTestRoot(TEXT("Catalog"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	FPRSafeSaveStore Store(TestRoot);
	UPRProfileCatalogSave* Catalog = NewObject<UPRProfileCatalogSave>(GetTransientPackage());
	const FGuid FirstId = FGuid::NewGuid();
	const FGuid SecondId = FGuid::NewGuid();
	Catalog->ProfileIds = { FirstId, SecondId };
	Catalog->ActiveProfileId = FirstId;
	TestTrue(TEXT("Initial catalog save succeeds"), Store.SaveCatalog(Catalog).IsSuccess());
	Catalog->ActiveProfileId = SecondId;
	TestTrue(TEXT("Second catalog save succeeds"), Store.SaveCatalog(Catalog).IsSuccess());

	const FString CatalogPrimary = FPaths::Combine(TestRoot, TEXT("catalog.primary.prsave"));
	TArray<uint8> CatalogBytes;
	FFileHelper::LoadFileToArray(CatalogBytes, *CatalogPrimary);
	CatalogBytes.SetNum(FMath::Max(1, CatalogBytes.Num() / 2));
	FFileHelper::SaveArrayToFile(CatalogBytes, *CatalogPrimary);
	UPRProfileCatalogSave* RecoveredCatalog = nullptr;
	const FPRProfileOperationResult RecoveryResult = Store.LoadCatalog(RecoveredCatalog);
	TestTrue(TEXT("Catalog backup recovery succeeds"), RecoveryResult.IsSuccess());
	TestTrue(TEXT("Catalog backup recovery is reported"), RecoveryResult.bRecoveredFromBackup);
	TestEqual(TEXT("Recovered catalog retains previous active profile"), RecoveredCatalog ? RecoveredCatalog->ActiveProfileId : FGuid(), FirstId);

	UPRProfileCatalogSave* FutureCatalog = NewObject<UPRProfileCatalogSave>(GetTransientPackage());
	FutureCatalog->CatalogVersion = 2;
	FutureCatalog->ProfileIds = Catalog->ProfileIds;
	FutureCatalog->ActiveProfileId = SecondId;
	TestTrue(TEXT("Can write a syntactically valid future catalog"), WriteRawEnvelopedSave(FutureCatalog, CatalogPrimary));
	TArray<uint8> FutureCatalogPrimaryBeforeLoad;
	TArray<uint8> FutureCatalogBackupBeforeLoad;
	FFileHelper::LoadFileToArray(FutureCatalogPrimaryBeforeLoad, *CatalogPrimary);
	FFileHelper::LoadFileToArray(FutureCatalogBackupBeforeLoad, *FPaths::Combine(TestRoot, TEXT("catalog.backup.prsave")));
	UPRProfileCatalogSave* RejectedFutureCatalog = nullptr;
	const FPRProfileOperationResult FutureCatalogResult = Store.LoadCatalog(RejectedFutureCatalog);
	TestEqual(TEXT("Future catalog is rejected without fallback"), FutureCatalogResult.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestFalse(TEXT("Future catalog does not recover an older backup"), FutureCatalogResult.bRecoveredFromBackup);
	TestNull(TEXT("Future catalog is not exposed"), RejectedFutureCatalog);
	const FPRProfileOperationResult CatalogSaveAgainstFuture = Store.SaveCatalog(Catalog);
	TestEqual(TEXT("Current writer refuses to replace a future catalog"), CatalogSaveAgainstFuture.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TArray<uint8> FutureCatalogPrimaryAfterSave;
	TArray<uint8> FutureCatalogBackupAfterSave;
	FFileHelper::LoadFileToArray(FutureCatalogPrimaryAfterSave, *CatalogPrimary);
	FFileHelper::LoadFileToArray(FutureCatalogBackupAfterSave, *FPaths::Combine(TestRoot, TEXT("catalog.backup.prsave")));
	TestTrue(TEXT("Future catalog primary remains byte-identical"), FutureCatalogPrimaryAfterSave == FutureCatalogPrimaryBeforeLoad);
	TestTrue(TEXT("Future catalog backup remains byte-identical"), FutureCatalogBackupAfterSave == FutureCatalogBackupBeforeLoad);
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);

	const FString SemanticCatalogRoot = MakeTestRoot(TEXT("SemanticCatalog"));
	FPRSafeSaveStore SemanticCatalogStore(SemanticCatalogRoot);
	UPRProfileCatalogSave* ValidCatalog = NewObject<UPRProfileCatalogSave>(GetTransientPackage());
	ValidCatalog->ActiveProfileId = FirstId;
	ValidCatalog->ProfileIds = { FirstId };
	TestTrue(TEXT("Semantic catalog first save succeeds"), SemanticCatalogStore.SaveCatalog(ValidCatalog).IsSuccess());
	ValidCatalog->ActiveProfileId = SecondId;
	ValidCatalog->ProfileIds.Add(SecondId);
	TestTrue(TEXT("Semantic catalog second save succeeds"), SemanticCatalogStore.SaveCatalog(ValidCatalog).IsSuccess());
	UPRProfileSave* WrongCatalogObject = MakeProfile(TEXT("Wrong Catalog Object"));
	const FString SemanticCatalogPrimary = FPaths::Combine(SemanticCatalogRoot, TEXT("catalog.primary.prsave"));
	TestTrue(TEXT("Can inject wrong object type into catalog primary"), WriteRawEnvelopedSave(WrongCatalogObject, SemanticCatalogPrimary));
	UPRProfileCatalogSave* SemanticallyRecoveredCatalog = nullptr;
	const FPRProfileOperationResult SemanticCatalogResult = SemanticCatalogStore.LoadCatalog(SemanticallyRecoveredCatalog);
	TestTrue(TEXT("Wrong catalog object type recovers backup"), SemanticCatalogResult.IsSuccess());
	TestTrue(TEXT("Semantic catalog recovery is reported"), SemanticCatalogResult.bRecoveredFromBackup);
	TestEqual(TEXT("Semantic catalog recovery returns prior active ID"), SemanticallyRecoveredCatalog ? SemanticallyRecoveredCatalog->ActiveProfileId : FGuid(), FirstId);
	TestTrue(TEXT("Wrong catalog primary is retained as corrupt"), IFileManager::Get().FileExists(*FPaths::Combine(SemanticCatalogRoot, TEXT("catalog.corrupt.prsave"))));
	IFileManager::Get().DeleteDirectory(*SemanticCatalogRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProfileRiftCaptureGuardTest,
	"ProjectRift.Save.RiftCaptureGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProfileRiftCaptureGuardTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Rift capture guard world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	World->GetWorldSettings()->DefaultGameMode = APRRiftGameMode::StaticClass();
	TestTrue(TEXT("Rift capture guard world begins play"), WorldWrapper.BeginPlayInTestWorld());
	APRPlayerState* RiftPlayerState = World->SpawnActor<APRPlayerState>();
	TestNotNull(TEXT("Rift player state exists"), RiftPlayerState);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(TestGameInstance);
	UPRProfileSave* ActiveProfile = MakeProfile(TEXT("Rift Guard"));
	FObjectProperty* ActiveProfileProperty = FindFProperty<FObjectProperty>(UPRSaveSubsystem::StaticClass(), TEXT("ActiveProfile"));
	TestNotNull(TEXT("Subsystem owns an active profile reference"), ActiveProfileProperty);
	if (ActiveProfileProperty)
	{
		ActiveProfileProperty->SetObjectPropertyValue_InContainer(SaveSubsystem, ActiveProfile);
	}
	AddExpectedError(TEXT("Profile operation failed. Status=7"), EAutomationExpectedErrorFlags::Contains, 1);
	const FPRProfileOperationResult Result = SaveSubsystem->CaptureAndSaveAtSafeCheckpoint(RiftPlayerState);
	TestEqual(TEXT("Rift runtime capture is rejected"), Result.Status, EPRProfileOperationStatus::UnsafeCaptureContext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProfileSubsystemIntegrationTest,
	"ProjectRift.Save.SubsystemIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProfileSubsystemIntegrationTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeTestRoot(TEXT("SubsystemIntegration"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* FirstGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* FirstSubsystem = NewObject<UPRSaveSubsystem>(FirstGameInstance);
	FirstSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult FirstCreate = FirstSubsystem->CreateProfile(TEXT("Alpha"));
	const FPRProfileOperationResult SecondCreate = FirstSubsystem->CreateProfile(TEXT("Beta"));
	TestTrue(TEXT("First integrated profile is created"), FirstCreate.IsSuccess());
	TestTrue(TEXT("Second integrated profile is created"), SecondCreate.IsSuccess());
	TestEqual(TEXT("Integrated subsystem lists two profiles"), FirstSubsystem->GetProfiles().Num(), 2);
	TestEqual(TEXT("Newest profile becomes active"), FirstSubsystem->GetActiveProfileId(), SecondCreate.ProfileId);

	APRPlayerState* SourcePlayerState = NewObject<APRPlayerState>(GetTransientPackage());
	SourcePlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	SourcePlayerState->AddShipResource(TEXT("EnergyCrystal"), 13);
	SourcePlayerState->GetInventoryComponent()->AddItem(MakeProfileTestItem(TEXT("HealthInjector"), 1));
	TestTrue(TEXT("Safe checkpoint captures and saves through real subsystem"), FirstSubsystem->CaptureAndSaveAtSafeCheckpoint(SourcePlayerState).IsSuccess());
	APRPlayerState* AppliedPlayerState = NewObject<APRPlayerState>(GetTransientPackage());
	TestTrue(TEXT("Real subsystem applies active snapshot"), FirstSubsystem->ApplyActiveProfileToPlayerState(AppliedPlayerState).IsSuccess());
	TestEqual(TEXT("Applied subsystem resource count"), AppliedPlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 13);
	FirstSubsystem->Deinitialize();

	FPRSafeSaveStore ExternalStore(TestRoot);
	UPRProfileCatalogSave* DuplicateCatalog = nullptr;
	TestTrue(TEXT("Can load catalog before duplicate recovery test"), ExternalStore.LoadCatalog(DuplicateCatalog).IsSuccess());
	DuplicateCatalog->ProfileIds.Add(FirstCreate.ProfileId);
	TestTrue(TEXT("Can persist duplicate GUID recovery fixture"), ExternalStore.SaveCatalog(DuplicateCatalog).IsSuccess());
	TestTrue(TEXT("Can simulate missing active profile directory"), ExternalStore.DeleteProfile(SecondCreate.ProfileId).IsSuccess());
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* RecoveredSubsystem = NewObject<UPRSaveSubsystem>(SecondGameInstance);
	RecoveredSubsystem->InitializeForAutomation(TestRoot);
	TestEqual(TEXT("Reconciled catalog removes missing profile"), RecoveredSubsystem->GetProfiles().Num(), 1);
	TestFalse(TEXT("Missing active profile clears active selection"), RecoveredSubsystem->GetActiveProfileId().IsValid());
	TestTrue(TEXT("Remaining profile can be activated"), RecoveredSubsystem->ActivateAndLoadProfile(FirstCreate.ProfileId).IsSuccess());
	TestTrue(TEXT("Active profile can be deleted"), RecoveredSubsystem->DeleteProfile(FirstCreate.ProfileId).IsSuccess());
	TestEqual(TEXT("Deleting last profile leaves an empty catalog"), RecoveredSubsystem->GetProfiles().Num(), 0);
	TestFalse(TEXT("Deleting active profile does not auto-select another"), RecoveredSubsystem->GetActiveProfileId().IsValid());
	RecoveredSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);

	const FString FutureRoot = MakeTestRoot(TEXT("FutureCatalogSubsystem"));
	FPRSafeSaveStore FutureStore(FutureRoot);
	UPRProfileCatalogSave* CurrentCatalog = NewObject<UPRProfileCatalogSave>(GetTransientPackage());
	TestTrue(TEXT("Future setup catalog save one succeeds"), FutureStore.SaveCatalog(CurrentCatalog).IsSuccess());
	TestTrue(TEXT("Future setup catalog save two succeeds"), FutureStore.SaveCatalog(CurrentCatalog).IsSuccess());
	UPRProfileCatalogSave* FutureCatalog = NewObject<UPRProfileCatalogSave>(GetTransientPackage());
	FutureCatalog->CatalogVersion = 2;
	const FString FutureCatalogPrimary = FPaths::Combine(FutureRoot, TEXT("catalog.primary.prsave"));
	TestTrue(TEXT("Can inject future catalog for subsystem startup"), WriteRawEnvelopedSave(FutureCatalog, FutureCatalogPrimary));
	TArray<uint8> FuturePrimaryBeforeSubsystem;
	TArray<uint8> FutureBackupBeforeSubsystem;
	FFileHelper::LoadFileToArray(FuturePrimaryBeforeSubsystem, *FutureCatalogPrimary);
	FFileHelper::LoadFileToArray(FutureBackupBeforeSubsystem, *FPaths::Combine(FutureRoot, TEXT("catalog.backup.prsave")));
	UGameInstance* FutureGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* FutureSubsystem = NewObject<UPRSaveSubsystem>(FutureGameInstance);
	AddExpectedError(TEXT("Profile operation failed. Status=4"), EAutomationExpectedErrorFlags::Contains, 1);
	FutureSubsystem->InitializeForAutomation(FutureRoot);
	TestEqual(TEXT("Subsystem startup rejects future catalog"), FutureSubsystem->GetLastOperationResult().Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestEqual(TEXT("Future catalog exposes no downgrade profile list"), FutureSubsystem->GetProfiles().Num(), 0);
	AddExpectedError(TEXT("Profile operation failed. Status=9"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Create is blocked while future catalog owns the root"), FutureSubsystem->CreateProfile(TEXT("Blocked")).IsSuccess());
	TArray<uint8> FuturePrimaryAfterSubsystem;
	TArray<uint8> FutureBackupAfterSubsystem;
	FFileHelper::LoadFileToArray(FuturePrimaryAfterSubsystem, *FutureCatalogPrimary);
	FFileHelper::LoadFileToArray(FutureBackupAfterSubsystem, *FPaths::Combine(FutureRoot, TEXT("catalog.backup.prsave")));
	TestTrue(TEXT("Subsystem leaves future catalog primary byte-identical"), FuturePrimaryAfterSubsystem == FuturePrimaryBeforeSubsystem);
	TestTrue(TEXT("Subsystem leaves future catalog backup byte-identical"), FutureBackupAfterSubsystem == FutureBackupBeforeSubsystem);
	FutureSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*FutureRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMultiplayerSettlementPersistenceTest,
	"ProjectRift.MultiplayerProfile.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMultiplayerSettlementPersistenceTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeTestRoot(TEXT("MultiplayerSettlement"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	SaveSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult CreateResult = SaveSubsystem->CreateProfile(TEXT("Settlement Client"));
	TestTrue(TEXT("Settlement test profile is created"), CreateResult.IsSuccess());

	FPRProfileSnapshot InitialSnapshot;
	InitialSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 5 } };
	InitialSnapshot.BackpackItems = { MakeProfileTestItem(TEXT("HealthInjector"), 2) };
	InitialSnapshot.UnlockedRoleIds = { TEXT("Ability.Role.Assault") };
	InitialSnapshot.SelectedRoleId = TEXT("Ability.Role.Assault");
	TestTrue(TEXT("Initial multiplayer snapshot is installed"), SaveSubsystem->ReplaceActiveProfileSnapshot(InitialSnapshot).IsSuccess());
	TestTrue(TEXT("Initial multiplayer snapshot is saved"), SaveSubsystem->SaveActiveProfile().IsSuccess());

	FPRMultiplayerProfileProjection Projection;
	TestTrue(TEXT("Active profile builds a multiplayer projection"), SaveSubsystem->BuildMultiplayerProfileProjection(Projection).IsSuccess());
	TestEqual(TEXT("Projection binds the active profile id"), Projection.ProfileId, CreateResult.ProfileId);
	TestTrue(TEXT("Active profile can be locked to the session"), SaveSubsystem->BindActiveProfileToSession().IsSuccess());
	TestTrue(TEXT("Session binding is visible"), SaveSubsystem->IsSessionProfileBound());
	TestFalse(TEXT("Bound profile cannot be deleted"), SaveSubsystem->DeleteProfile(CreateResult.ProfileId).IsSuccess());
	TestFalse(TEXT("Bound profile cannot be externally replaced"), SaveSubsystem->ReplaceActiveProfileSnapshot(FPRProfileSnapshot()).IsSuccess());
	TestEqual(
		TEXT("Bound profile cannot be mutated through a lobby checkpoint"),
		SaveSubsystem->CaptureAndSaveAtSafeCheckpoint(nullptr).Status,
		EPRProfileOperationStatus::Busy);

	FPRPlayerSettlementReceipt Receipt;
	Receipt.ProfileId = CreateResult.ProfileId;
	Receipt.MissionId = TEXT("Mission.Rift.Test.Hold");
	Receipt.RunId = FGuid::NewGuid();
	Receipt.SettlementId = FGuid::NewGuid();
	Receipt.Result = EPRRiftMissionResult::Success;
	Receipt.bExtracted = true;
	Receipt.bGrantStoryProgress = true;
	Receipt.SettledBackpackItems = { MakeProfileTestItem(TEXT("ShieldPack"), 1) };
	Receipt.SettledEquipment = { FPRProfileEquipmentEntry(TEXT("Slot.Primary"), MakeProfileTestItem(TEXT("TestRifle"), 1)) };
	Receipt.SettledResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 12 } };
	Receipt.SettledRoleId = TEXT("Ability.Role.Assault");
	const FPRProfileOperationResult ApplyResult = SaveSubsystem->ApplyMultiplayerSettlementReceipt(Receipt);
	TestTrue(TEXT("Valid personal settlement is saved transactionally"), ApplyResult.IsSuccess());
	TestFalse(TEXT("First settlement apply is not marked duplicate"), ApplyResult.bAlreadyProcessedSettlement);

	FPRProfileSnapshot AppliedSnapshot;
	TestTrue(TEXT("Applied multiplayer snapshot is available"), SaveSubsystem->GetActiveProfileSnapshot(AppliedSnapshot));
	TestEqual(TEXT("Settled resource wallet replaces runtime wallet"), AppliedSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 12);
	TestEqual(TEXT("Settled backpack replaces runtime backpack"), AppliedSnapshot.BackpackItems.Num(), 1);
	TestEqual(TEXT("Settled primary equipment replaces the snapshot primary slot"), AppliedSnapshot.Equipment.Num(), 1);
	TestEqual(TEXT("Settled primary equipment keeps the rifle"), AppliedSnapshot.Equipment[0].Item.ItemId, FName(TEXT("TestRifle")));
	TestTrue(TEXT("Settlement id is recorded durably"), AppliedSnapshot.ProcessedSettlementIds.Contains(Receipt.SettlementId));
	TestTrue(TEXT("Eligible extracted player receives story completion"), AppliedSnapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftTestHold")));

	const FPRProfileOperationResult DuplicateResult = SaveSubsystem->ApplyMultiplayerSettlementReceipt(Receipt);
	TestTrue(TEXT("Duplicate settlement is an idempotent success"), DuplicateResult.IsSuccess());
	TestTrue(TEXT("Duplicate settlement is reported"), DuplicateResult.bAlreadyProcessedSettlement);

	FPRPlayerSettlementReceipt WrongProfileReceipt = Receipt;
	WrongProfileReceipt.SettlementId = FGuid::NewGuid();
	WrongProfileReceipt.ProfileId = FGuid::NewGuid();
	TestFalse(TEXT("Wrong profile receipt is rejected"), SaveSubsystem->ApplyMultiplayerSettlementReceipt(WrongProfileReceipt).IsSuccess());
	FPRProfileSnapshot AfterWrongProfile;
	SaveSubsystem->GetActiveProfileSnapshot(AfterWrongProfile);
	TestEqual(TEXT("Rejected receipt leaves wallet unchanged"), AfterWrongProfile.GetResourceCount(TEXT("EnergyCrystal")), 12);

	SaveSubsystem->ReleaseSessionProfileBinding();
	TestFalse(TEXT("Session binding can be released"), SaveSubsystem->IsSessionProfileBound());
	SaveSubsystem->Deinitialize();

	UGameInstance* ReloadGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* ReloadSubsystem = NewObject<UPRSaveSubsystem>(ReloadGameInstance);
	ReloadSubsystem->InitializeForAutomation(TestRoot);
	TestTrue(TEXT("Reloaded settlement profile activates"), ReloadSubsystem->ActivateAndLoadProfile(CreateResult.ProfileId).IsSuccess());
	FPRProfileSnapshot ReloadedSnapshot;
	ReloadSubsystem->GetActiveProfileSnapshot(ReloadedSnapshot);
	TestEqual(TEXT("Reload preserves settled wallet"), ReloadedSnapshot.GetResourceCount(TEXT("EnergyCrystal")), 12);
	TestEqual(TEXT("Reload preserves settled primary equipment"), ReloadedSnapshot.Equipment[0].Item.ItemId, FName(TEXT("TestRifle")));
	TestTrue(TEXT("Reload preserves processed settlement id"), ReloadedSnapshot.ProcessedSettlementIds.Contains(Receipt.SettlementId));
	ReloadSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMultiplayerSettlementWriteFailureTest,
	"ProjectRift.MultiplayerProfile.WriteFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMultiplayerSettlementWriteFailureTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = MakeTestRoot(TEXT("MultiplayerSettlementWriteFailure"));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRSaveSubsystem* SaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	SaveSubsystem->InitializeForAutomation(TestRoot);
	const FPRProfileOperationResult CreateResult = SaveSubsystem->CreateProfile(TEXT("Write Failure Client"));
	TestTrue(TEXT("Write-failure profile is created"), CreateResult.IsSuccess());

	FPRProfileSnapshot InitialSnapshot;
	InitialSnapshot.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 5 } };
	TestTrue(TEXT("Write-failure initial snapshot is installed"), SaveSubsystem->ReplaceActiveProfileSnapshot(InitialSnapshot).IsSuccess());
	TestTrue(TEXT("Write-failure initial snapshot is saved"), SaveSubsystem->SaveActiveProfile().IsSuccess());
	TestTrue(TEXT("Write-failure profile is session bound"), SaveSubsystem->BindActiveProfileToSession().IsSuccess());

	UPRProfileSave* FuturePrimary = MakeProfile(TEXT("Future Write Barrier"));
	FuturePrimary->ProfileId = CreateResult.ProfileId;
	FuturePrimary->SaveVersion = UPRProfileSave::LatestSaveVersion + 1;
	FPRSafeSaveStore Store(TestRoot);
	TestTrue(TEXT("Can install a future-version primary as a deterministic write barrier"), WriteRawEnvelopedSave(FuturePrimary, Store.GetProfilePrimaryPath(CreateResult.ProfileId)));

	FPRPlayerSettlementReceipt Receipt;
	Receipt.ProfileId = CreateResult.ProfileId;
	Receipt.MissionId = TEXT("Mission.Rift.Test.Hold");
	Receipt.RunId = FGuid::NewGuid();
	Receipt.SettlementId = FGuid::NewGuid();
	Receipt.Result = EPRRiftMissionResult::Success;
	Receipt.bExtracted = true;
	Receipt.SettledResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 50 } };
	const FPRProfileOperationResult ApplyResult = SaveSubsystem->ApplyMultiplayerSettlementReceipt(Receipt);
	TestEqual(TEXT("Future primary rejects settlement write"), ApplyResult.Status, EPRProfileOperationStatus::UnsupportedFutureVersion);
	TestTrue(TEXT("Rejected settlement remains pending for lobby retry"), SaveSubsystem->HasPendingSettlementReceipt());
	FPRProfileSnapshot AfterFailure;
	SaveSubsystem->GetActiveProfileSnapshot(AfterFailure);
	TestEqual(TEXT("Rejected write preserves the in-memory active snapshot"), AfterFailure.GetResourceCount(TEXT("EnergyCrystal")), 5);
	TestFalse(TEXT("Rejected write does not record the settlement id in memory"), AfterFailure.ProcessedSettlementIds.Contains(Receipt.SettlementId));

	SaveSubsystem->Deinitialize();
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

#endif
