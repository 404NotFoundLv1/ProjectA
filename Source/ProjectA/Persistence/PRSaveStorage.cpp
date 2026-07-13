#include "Persistence/PRSaveStorage.h"

#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/PRProfileSave.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
constexpr uint32 ProfileEnvelopeMagic = 0x56535250;
constexpr uint16 ProfileEnvelopeVersion = 1;
constexpr int64 ProfileEnvelopeHeaderSize = sizeof(uint32) + sizeof(uint16) + sizeof(uint32) + sizeof(uint32);

bool MoveReplacing(const FString& Destination, const FString& Source)
{
	return IFileManager::Get().Move(*Destination, *Source, true, true, false, true);
}

bool IsFutureVersionSave(const USaveGame* SaveGame, int32& OutVersion)
{
	if (const UPRProfileSave* Profile = Cast<UPRProfileSave>(SaveGame))
	{
		OutVersion = Profile->SaveVersion;
		return Profile->SaveVersion > UPRProfileSave::LatestSaveVersion;
	}
	if (const UPRProfileCatalogSave* Catalog = Cast<UPRProfileCatalogSave>(SaveGame))
	{
		OutVersion = Catalog->CatalogVersion;
		return Catalog->CatalogVersion > UPRProfileCatalogSave::LatestCatalogVersion;
	}
	OutVersion = 0;
	return false;
}

bool IsProfileSemanticallyLoadable(const UPRProfileSave* Profile, const FGuid& ExpectedProfileId)
{
	if (!Profile
		|| Profile->ProfileId != ExpectedProfileId
		|| Profile->SaveVersion < UPRProfileSave::InitialSaveVersion
		|| Profile->SaveVersion > UPRProfileSave::LatestSaveVersion
		|| Profile->DisplayName.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}
	return Profile->SaveVersion < UPRProfileSave::LatestSaveVersion || Profile->IsValid();
}

bool IsCompatibleForBackupRotation(const USaveGame* ExistingSave, const USaveGame* IncomingSave, const FGuid& ProfileId)
{
	if (Cast<UPRProfileSave>(IncomingSave))
	{
		const UPRProfileSave* ExistingProfile = Cast<UPRProfileSave>(ExistingSave);
		return IsProfileSemanticallyLoadable(ExistingProfile, ProfileId);
	}
	if (Cast<UPRProfileCatalogSave>(IncomingSave))
	{
		const UPRProfileCatalogSave* ExistingCatalog = Cast<UPRProfileCatalogSave>(ExistingSave);
		return ExistingCatalog && ExistingCatalog->CatalogVersion == UPRProfileCatalogSave::LatestCatalogVersion;
	}
	return false;
}
}

FPRSafeSaveStore::FPRSafeSaveStore(FString InRootDirectory)
	: RootDirectory(FPaths::ConvertRelativePathToFull(MoveTemp(InRootDirectory)))
{
	FPaths::NormalizeDirectoryName(RootDirectory);
}

FPRProfileOperationResult FPRSafeSaveStore::SaveProfile(UPRProfileSave* Profile)
{
	if (!Profile)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Profile is null."));
	}
	FString Diagnostic;
	if (!Profile->IsValid(&Diagnostic))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, Profile->ProfileId);
	}
	const FDateTime PreviousLastSavedUtc = Profile->LastSavedUtc;
	Profile->LastSavedUtc = FDateTime::UtcNow();
	FPRProfileOperationResult Result = SaveObject(Profile, GetProfileFiles(Profile->ProfileId), Profile->ProfileId);
	if (!Result.IsSuccess())
	{
		Profile->LastSavedUtc = PreviousLastSavedUtc;
	}
	return Result;
}

FPRProfileOperationResult FPRSafeSaveStore::LoadProfile(const FGuid& ProfileId, UPRProfileSave*& OutProfile)
{
	OutProfile = nullptr;
	if (!ProfileId.IsValid())
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("ProfileId is invalid."));
	}
	const FFileSet Files = GetProfileFiles(ProfileId);
	USaveGame* LoadedObject = nullptr;
	FPRProfileOperationResult Result = LoadObject(Files, LoadedObject, ProfileId);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutProfile = Cast<UPRProfileSave>(LoadedObject);
	if (OutProfile && OutProfile->SaveVersion > UPRProfileSave::LatestSaveVersion)
	{
		Result.LoadedVersion = OutProfile->SaveVersion;
		OutProfile = nullptr;
		Result.Status = EPRProfileOperationStatus::UnsupportedFutureVersion;
		Result.Diagnostic = TEXT("The profile was created by a newer save schema.");
		return Result;
	}

	const bool bPrimarySemanticDataValid = IsProfileSemanticallyLoadable(OutProfile, ProfileId);
	if (!bPrimarySemanticDataValid)
	{
		if (Result.bRecoveredFromBackup || !IFileManager::Get().FileExists(*Files.Backup))
		{
			OutProfile = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Profile type, ProfileId, or save version does not match its directory."), ProfileId);
		}

		USaveGame* BackupObject = nullptr;
		FString BackupDiagnostic;
		if (DecodeSaveGame(Files.Backup, BackupObject, BackupDiagnostic) != EPRProfileOperationStatus::Success)
		{
			OutProfile = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Primary profile semantics are invalid and the backup is unreadable."), ProfileId);
		}
		UPRProfileSave* BackupProfile = Cast<UPRProfileSave>(BackupObject);
		if (BackupProfile && BackupProfile->SaveVersion > UPRProfileSave::LatestSaveVersion)
		{
			OutProfile = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::UnsupportedFutureVersion, TEXT("The only semantically valid backup uses a newer save schema."), ProfileId);
		}
		if (!IsProfileSemanticallyLoadable(BackupProfile, ProfileId))
		{
			OutProfile = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Primary and backup profile semantics are invalid."), ProfileId);
		}

		IFileManager& FileManager = IFileManager::Get();
		FileManager.Delete(*Files.Corrupt, false, true);
		if (!MoveReplacing(Files.Corrupt, Files.Primary) || !RepairPrimaryFromBackup(Files))
		{
			OutProfile = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("The semantic backup was valid but the primary profile could not be repaired."), ProfileId);
		}
		OutProfile = BackupProfile;
		Result = FPRProfileOperationResult::MakeSuccess(ProfileId);
		Result.bRecoveredFromBackup = true;
		Result.Diagnostic = TEXT("Primary profile semantics were invalid; a validated backup was restored.");
	}

	Result.LoadedVersion = OutProfile->SaveVersion;
	if (OutProfile->SaveVersion > UPRProfileSave::LatestSaveVersion)
	{
		OutProfile = nullptr;
		Result.Status = EPRProfileOperationStatus::UnsupportedFutureVersion;
		Result.Diagnostic = TEXT("The profile was created by a newer save schema.");
	}
	return Result;
}

FPRProfileOperationResult FPRSafeSaveStore::DeleteProfile(const FGuid& ProfileId)
{
	if (!ProfileId.IsValid())
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("ProfileId is invalid."));
	}
	const FString Directory = FPaths::GetPath(GetProfilePrimaryPath(ProfileId));
	if (!IsPathInsideRoot(Directory))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Profile directory is outside the ProjectA save root."), ProfileId);
	}
	if (IFileManager::Get().DirectoryExists(*Directory) && !IFileManager::Get().DeleteDirectory(*Directory, false, true))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not delete the profile directory."), ProfileId);
	}
	return FPRProfileOperationResult::MakeSuccess(ProfileId);
}

TArray<FGuid> FPRSafeSaveStore::DiscoverProfileIds() const
{
	TArray<FGuid> Result;
	const FString ProfilesDirectory = FPaths::Combine(RootDirectory, TEXT("Profiles"));
	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *FPaths::Combine(ProfilesDirectory, TEXT("*")), false, true);
	for (const FString& DirectoryName : Directories)
	{
		FGuid ProfileId;
		if (FGuid::ParseExact(DirectoryName, EGuidFormats::Digits, ProfileId))
		{
			Result.Add(ProfileId);
		}
	}
	return Result;
}

FPRProfileOperationResult FPRSafeSaveStore::SaveCatalog(UPRProfileCatalogSave* Catalog)
{
	if (!Catalog)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Profile catalog is null."));
	}
	if (Catalog->CatalogVersion > UPRProfileCatalogSave::LatestCatalogVersion)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::UnsupportedFutureVersion, TEXT("The profile catalog uses a newer schema."));
	}
	return SaveObject(Catalog, GetCatalogFiles(), FGuid());
}

FPRProfileOperationResult FPRSafeSaveStore::LoadCatalog(UPRProfileCatalogSave*& OutCatalog)
{
	OutCatalog = nullptr;
	const FFileSet Files = GetCatalogFiles();
	USaveGame* LoadedObject = nullptr;
	FPRProfileOperationResult Result = LoadObject(Files, LoadedObject, FGuid());
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutCatalog = Cast<UPRProfileCatalogSave>(LoadedObject);
	if (OutCatalog && OutCatalog->CatalogVersion > UPRProfileCatalogSave::LatestCatalogVersion)
	{
		Result.Status = EPRProfileOperationStatus::UnsupportedFutureVersion;
		Result.LoadedVersion = OutCatalog->CatalogVersion;
		Result.Diagnostic = TEXT("The profile catalog was created by a newer save schema.");
		OutCatalog = nullptr;
		return Result;
	}
	if (!OutCatalog || OutCatalog->CatalogVersion != UPRProfileCatalogSave::LatestCatalogVersion)
	{
		if (Result.bRecoveredFromBackup || !IFileManager::Get().FileExists(*Files.Backup))
		{
			OutCatalog = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Profile catalog type or version is invalid."));
		}
		USaveGame* BackupObject = nullptr;
		FString BackupDiagnostic;
		if (DecodeSaveGame(Files.Backup, BackupObject, BackupDiagnostic) != EPRProfileOperationStatus::Success)
		{
			OutCatalog = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Catalog semantics are invalid and the backup is unreadable."));
		}
		UPRProfileCatalogSave* BackupCatalog = Cast<UPRProfileCatalogSave>(BackupObject);
		if (BackupCatalog && BackupCatalog->CatalogVersion > UPRProfileCatalogSave::LatestCatalogVersion)
		{
			OutCatalog = nullptr;
			FPRProfileOperationResult FutureResult = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::UnsupportedFutureVersion, TEXT("The only valid catalog backup uses a newer schema."));
			FutureResult.LoadedVersion = BackupCatalog->CatalogVersion;
			return FutureResult;
		}
		if (!BackupCatalog || BackupCatalog->CatalogVersion != UPRProfileCatalogSave::LatestCatalogVersion)
		{
			OutCatalog = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Primary and backup catalog semantics are invalid."));
		}
		IFileManager& FileManager = IFileManager::Get();
		FileManager.Delete(*Files.Corrupt, false, true);
		if (!MoveReplacing(Files.Corrupt, Files.Primary) || !RepairPrimaryFromBackup(Files))
		{
			OutCatalog = nullptr;
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("The valid catalog backup could not repair the primary catalog."));
		}
		OutCatalog = BackupCatalog;
		Result = FPRProfileOperationResult::MakeSuccess();
		Result.bRecoveredFromBackup = true;
		Result.Diagnostic = TEXT("Primary catalog semantics were invalid; a validated backup was restored.");
	}
	return Result;
}

FString FPRSafeSaveStore::GetProfilePrimaryPath(const FGuid& ProfileId) const { return GetProfileFiles(ProfileId).Primary; }
FString FPRSafeSaveStore::GetProfileBackupPath(const FGuid& ProfileId) const { return GetProfileFiles(ProfileId).Backup; }
FString FPRSafeSaveStore::GetProfileCorruptPath(const FGuid& ProfileId) const { return GetProfileFiles(ProfileId).Corrupt; }
FString FPRSafeSaveStore::GetProfileTempPath(const FGuid& ProfileId) const { return GetProfileFiles(ProfileId).Temp; }

FPRSafeSaveStore::FFileSet FPRSafeSaveStore::GetProfileFiles(const FGuid& ProfileId) const
{
	const FString Directory = FPaths::Combine(RootDirectory, TEXT("Profiles"), ProfileId.ToString(EGuidFormats::Digits));
	return {
		FPaths::Combine(Directory, TEXT("profile.primary.prsave")),
		FPaths::Combine(Directory, TEXT("profile.backup.prsave")),
		FPaths::Combine(Directory, TEXT("profile.tmp.prsave")),
		FPaths::Combine(Directory, TEXT("profile.corrupt.prsave"))
	};
}

FPRSafeSaveStore::FFileSet FPRSafeSaveStore::GetCatalogFiles() const
{
	return {
		FPaths::Combine(RootDirectory, TEXT("catalog.primary.prsave")),
		FPaths::Combine(RootDirectory, TEXT("catalog.backup.prsave")),
		FPaths::Combine(RootDirectory, TEXT("catalog.tmp.prsave")),
		FPaths::Combine(RootDirectory, TEXT("catalog.corrupt.prsave"))
	};
}

FPRProfileOperationResult FPRSafeSaveStore::SaveObject(USaveGame* SaveGame, const FFileSet& Files, const FGuid& ProfileId)
{
	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.FileExists(*Files.Primary))
	{
		USaveGame* ExistingSave = nullptr;
		FString ExistingDiagnostic;
		if (DecodeSaveGame(Files.Primary, ExistingSave, ExistingDiagnostic) == EPRProfileOperationStatus::Success)
		{
			int32 ExistingVersion = 0;
			if (IsFutureVersionSave(ExistingSave, ExistingVersion))
			{
				FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
					EPRProfileOperationStatus::UnsupportedFutureVersion,
					TEXT("Refusing to overwrite a primary save created by a newer schema."),
					ProfileId);
				Result.LoadedVersion = ExistingVersion;
				return Result;
			}
		}
	}
	if (!FileManager.MakeDirectory(*FPaths::GetPath(Files.Primary), true))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not create the save directory."), ProfileId);
	}

	TArray<uint8> EncodedBytes;
	FString Diagnostic;
	if (!EncodeSaveGame(SaveGame, EncodedBytes, Diagnostic)
		|| !FFileHelper::SaveArrayToFile(EncodedBytes, *Files.Temp))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, Diagnostic.IsEmpty() ? TEXT("Could not write the temporary save file.") : Diagnostic, ProfileId);
	}

	USaveGame* TempValidation = nullptr;
	if (DecodeSaveGame(Files.Temp, TempValidation, Diagnostic) != EPRProfileOperationStatus::Success)
	{
		FileManager.Delete(*Files.Temp, false, true);
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Temporary save validation failed: ") + Diagnostic, ProfileId);
	}

	if (FileManager.FileExists(*Files.Primary))
	{
		USaveGame* ExistingPrimary = nullptr;
		FString ExistingDiagnostic;
		if (DecodeSaveGame(Files.Primary, ExistingPrimary, ExistingDiagnostic) == EPRProfileOperationStatus::Success
			&& IsCompatibleForBackupRotation(ExistingPrimary, SaveGame, ProfileId))
		{
			FileManager.Delete(*Files.Backup, false, true);
			if (!MoveReplacing(Files.Backup, Files.Primary))
			{
				FileManager.Delete(*Files.Temp, false, true);
				return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not rotate the previous primary save to backup."), ProfileId);
			}
		}
		else
		{
			FileManager.Delete(*Files.Corrupt, false, true);
			if (!MoveReplacing(Files.Corrupt, Files.Primary))
			{
				FileManager.Delete(*Files.Temp, false, true);
				return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not quarantine the corrupt primary save."), ProfileId);
			}
		}
	}

	if (!MoveReplacing(Files.Primary, Files.Temp))
	{
		RepairPrimaryFromBackup(Files);
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not promote the temporary save to primary."), ProfileId);
	}

	USaveGame* FinalValidation = nullptr;
	if (DecodeSaveGame(Files.Primary, FinalValidation, Diagnostic) != EPRProfileOperationStatus::Success)
	{
		FileManager.Delete(*Files.Primary, false, true);
		RepairPrimaryFromBackup(Files);
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Final primary save validation failed."), ProfileId);
	}
	return FPRProfileOperationResult::MakeSuccess(ProfileId);
}

FPRProfileOperationResult FPRSafeSaveStore::LoadObject(const FFileSet& Files, USaveGame*& OutSaveGame, const FGuid& ProfileId)
{
	OutSaveGame = nullptr;
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*Files.Primary))
	{
		if (!FileManager.FileExists(*Files.Backup))
		{
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NotFound, TEXT("No primary or backup save exists."), ProfileId);
		}
		FString BackupDiagnostic;
		if (DecodeSaveGame(Files.Backup, OutSaveGame, BackupDiagnostic) != EPRProfileOperationStatus::Success)
		{
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("The backup save is invalid: ") + BackupDiagnostic, ProfileId);
		}
		if (!RepairPrimaryFromBackup(Files))
		{
			return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Backup loaded but the primary save could not be repaired."), ProfileId);
		}
		FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(ProfileId);
		Result.bRecoveredFromBackup = true;
		return Result;
	}

	FString PrimaryDiagnostic;
	if (DecodeSaveGame(Files.Primary, OutSaveGame, PrimaryDiagnostic) == EPRProfileOperationStatus::Success)
	{
		FileManager.Delete(*Files.Temp, false, true);
		return FPRProfileOperationResult::MakeSuccess(ProfileId);
	}

	if (!FileManager.FileExists(*Files.Backup))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Primary save is invalid and no backup exists: ") + PrimaryDiagnostic, ProfileId);
	}
	FString BackupDiagnostic;
	if (DecodeSaveGame(Files.Backup, OutSaveGame, BackupDiagnostic) != EPRProfileOperationStatus::Success)
	{
		OutSaveGame = nullptr;
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Corrupt, TEXT("Primary and backup saves are invalid."), ProfileId);
	}

	FileManager.Delete(*Files.Corrupt, false, true);
	if (!MoveReplacing(Files.Corrupt, Files.Primary) || !RepairPrimaryFromBackup(Files))
	{
		OutSaveGame = nullptr;
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Backup loaded but the corrupt primary could not be quarantined and repaired."), ProfileId);
	}
	FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(ProfileId);
	Result.bRecoveredFromBackup = true;
	Result.Diagnostic = TEXT("Primary save was corrupt; a validated backup was loaded and restored.");
	return Result;
}

bool FPRSafeSaveStore::EncodeSaveGame(USaveGame* SaveGame, TArray<uint8>& OutBytes, FString& OutDiagnostic) const
{
	TArray<uint8> Payload;
	if (!SaveGame || !UGameplayStatics::SaveGameToMemory(SaveGame, Payload))
	{
		OutDiagnostic = TEXT("Unreal SaveGame serialization failed.");
		return false;
	}
	const uint32 PayloadSize = Payload.Num();
	const uint32 PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());
	uint32 Magic = ProfileEnvelopeMagic;
	uint16 EnvelopeVersion = ProfileEnvelopeVersion;
	OutBytes.Reset(ProfileEnvelopeHeaderSize + Payload.Num());
	FMemoryWriter Writer(OutBytes, true);
	Writer << Magic;
	Writer << EnvelopeVersion;
	uint32 MutablePayloadSize = PayloadSize;
	uint32 MutablePayloadCrc = PayloadCrc;
	Writer << MutablePayloadSize;
	Writer << MutablePayloadCrc;
	Writer.Serialize(Payload.GetData(), Payload.Num());
	return !Writer.IsError();
}

EPRProfileOperationStatus FPRSafeSaveStore::DecodeSaveGame(const FString& Path, USaveGame*& OutSaveGame, FString& OutDiagnostic) const
{
	OutSaveGame = nullptr;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutDiagnostic = TEXT("Save file could not be read.");
		return EPRProfileOperationStatus::IOError;
	}
	if (Bytes.Num() < ProfileEnvelopeHeaderSize)
	{
		OutDiagnostic = TEXT("Save envelope is truncated.");
		return EPRProfileOperationStatus::Corrupt;
	}
	FMemoryReader Reader(Bytes, true);
	uint32 Magic = 0;
	uint16 EnvelopeVersion = 0;
	uint32 PayloadSize = 0;
	uint32 ExpectedCrc = 0;
	Reader << Magic;
	Reader << EnvelopeVersion;
	Reader << PayloadSize;
	Reader << ExpectedCrc;
	if (Magic != ProfileEnvelopeMagic || EnvelopeVersion != ProfileEnvelopeVersion)
	{
		OutDiagnostic = TEXT("Save envelope magic or version is invalid.");
		return EPRProfileOperationStatus::Corrupt;
	}
	if (PayloadSize != static_cast<uint32>(Bytes.Num() - Reader.Tell()))
	{
		OutDiagnostic = TEXT("Save payload length does not match the envelope.");
		return EPRProfileOperationStatus::Corrupt;
	}
	TArray<uint8> Payload;
	Payload.SetNumUninitialized(PayloadSize);
	Reader.Serialize(Payload.GetData(), PayloadSize);
	if (Reader.IsError() || FCrc::MemCrc32(Payload.GetData(), Payload.Num()) != ExpectedCrc)
	{
		OutDiagnostic = TEXT("Save payload checksum is invalid.");
		return EPRProfileOperationStatus::Corrupt;
	}
	OutSaveGame = UGameplayStatics::LoadGameFromMemory(Payload);
	if (!OutSaveGame)
	{
		OutDiagnostic = TEXT("Unreal SaveGame deserialization failed.");
		return EPRProfileOperationStatus::Corrupt;
	}
	return EPRProfileOperationStatus::Success;
}

bool FPRSafeSaveStore::RepairPrimaryFromBackup(const FFileSet& Files) const
{
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*Files.Backup))
	{
		return false;
	}
	FileManager.Delete(*Files.Temp, false, true);
	if (FileManager.Copy(*Files.Temp, *Files.Backup, true, true) != COPY_OK)
	{
		return false;
	}
	return MoveReplacing(Files.Primary, Files.Temp);
}

bool FPRSafeSaveStore::IsPathInsideRoot(const FString& Path) const
{
	FString FullRoot = RootDirectory;
	FString FullPath = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeDirectoryName(FullRoot);
	FPaths::NormalizeDirectoryName(FullPath);
	FullRoot += TEXT("/");
	return FullPath.StartsWith(FullRoot, ESearchCase::IgnoreCase);
}
