#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileTypes.h"

class UPRProfileCatalogSave;
class UPRProfileSave;
class USaveGame;

class PROJECTA_API FPRSafeSaveStore
{
public:
	explicit FPRSafeSaveStore(FString InRootDirectory);

	FPRProfileOperationResult SaveProfile(UPRProfileSave* Profile);
	FPRProfileOperationResult LoadProfile(const FGuid& ProfileId, UPRProfileSave*& OutProfile);
	FPRProfileOperationResult DeleteProfile(const FGuid& ProfileId);
	TArray<FGuid> DiscoverProfileIds() const;

	FPRProfileOperationResult SaveCatalog(UPRProfileCatalogSave* Catalog);
	FPRProfileOperationResult LoadCatalog(UPRProfileCatalogSave*& OutCatalog);

	FString GetProfilePrimaryPath(const FGuid& ProfileId) const;
	FString GetProfileBackupPath(const FGuid& ProfileId) const;
	FString GetProfileCorruptPath(const FGuid& ProfileId) const;
	FString GetProfileTempPath(const FGuid& ProfileId) const;
	const FString& GetRootDirectory() const { return RootDirectory; }

private:
	struct FFileSet
	{
		FString Primary;
		FString Backup;
		FString Temp;
		FString Corrupt;
	};

	FFileSet GetProfileFiles(const FGuid& ProfileId) const;
	FFileSet GetCatalogFiles() const;
	FPRProfileOperationResult SaveObject(USaveGame* SaveGame, const FFileSet& Files, const FGuid& ProfileId);
	FPRProfileOperationResult LoadObject(const FFileSet& Files, USaveGame*& OutSaveGame, const FGuid& ProfileId);
	bool EncodeSaveGame(USaveGame* SaveGame, TArray<uint8>& OutBytes, FString& OutDiagnostic) const;
	EPRProfileOperationStatus DecodeSaveGame(const FString& Path, USaveGame*& OutSaveGame, FString& OutDiagnostic) const;
	bool RepairPrimaryFromBackup(const FFileSet& Files) const;
	bool IsPathInsideRoot(const FString& Path) const;

	FString RootDirectory;
};
