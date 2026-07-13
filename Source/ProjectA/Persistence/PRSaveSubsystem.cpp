#include "Persistence/PRSaveSubsystem.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/PRProfileRuntimeBridge.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"

void UPRSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SaveStore = MakeUnique<FPRSafeSaveStore>(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("ProjectRift")));
	LoadOrRebuildCatalog();
	if (Catalog && Catalog->ActiveProfileId.IsValid())
	{
		ActivateAndLoadProfile(Catalog->ActiveProfileId);
	}
}

void UPRSaveSubsystem::Deinitialize()
{
	ActiveProfile = nullptr;
	Catalog = nullptr;
	SaveStore.Reset();
	Super::Deinitialize();
}

#if WITH_DEV_AUTOMATION_TESTS
void UPRSaveSubsystem::InitializeForAutomation(const FString& RootDirectory)
{
	ActiveProfile = nullptr;
	Catalog = nullptr;
	bHasAppliedActiveProfile = false;
	SaveStore = MakeUnique<FPRSafeSaveStore>(RootDirectory);
	LoadOrRebuildCatalog();
	if (Catalog && Catalog->ActiveProfileId.IsValid())
	{
		ActivateAndLoadProfile(Catalog->ActiveProfileId);
	}
}
#endif

FPRProfileOperationResult UPRSaveSubsystem::CreateProfile(const FString& DisplayName)
{
	if (!SaveStore || !Catalog)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Profile storage is not initialized."));
		SetLastResult(Result);
		return Result;
	}

	UPRProfileSave* NewProfile = NewObject<UPRProfileSave>(this);
	NewProfile->ProfileId = FGuid::NewGuid();
	FString NormalizedName = DisplayName.TrimStartAndEnd().Left(32);
	if (NormalizedName.IsEmpty())
	{
		NormalizedName = FString::Printf(TEXT("Profile %d"), Catalog->ProfileIds.Num() + 1);
	}
	NewProfile->DisplayName = NormalizedName;
	NewProfile->SaveVersion = UPRProfileSave::LatestSaveVersion;
	NewProfile->CreatedUtc = FDateTime::UtcNow();
	NewProfile->LastSavedUtc = NewProfile->CreatedUtc;

	FPRProfileOperationResult Result = SaveStore->SaveProfile(NewProfile);
	if (!Result.IsSuccess())
	{
		SetLastResult(Result);
		return Result;
	}
	UPRProfileCatalogSave* CandidateCatalog = DuplicateObject<UPRProfileCatalogSave>(Catalog, this);
	CandidateCatalog->ProfileIds.AddUnique(NewProfile->ProfileId);
	CandidateCatalog->ActiveProfileId = NewProfile->ProfileId;
	const FPRProfileOperationResult CatalogResult = SaveStore->SaveCatalog(CandidateCatalog);
	if (!CatalogResult.IsSuccess())
	{
		SaveStore->DeleteProfile(NewProfile->ProfileId);
		SetLastResult(CatalogResult);
		return CatalogResult;
	}
	Catalog = CandidateCatalog;
	ActiveProfile = NewProfile;
	bHasAppliedActiveProfile = false;
	SetLastResult(Result);
	return Result;
}

TArray<FPRProfileSummary> UPRSaveSubsystem::GetProfiles()
{
	TArray<FPRProfileSummary> Summaries;
	if (!SaveStore || !Catalog)
	{
		return Summaries;
	}
	for (const FGuid& ProfileId : Catalog->ProfileIds)
	{
		UPRProfileSave* Profile = nullptr;
		const FPRProfileOperationResult Result = SaveStore->LoadProfile(ProfileId, Profile);
		if (Result.IsSuccess() && Profile)
		{
			Summaries.Add(Profile->MakeSummary(ProfileId == Catalog->ActiveProfileId));
		}
	}
	Summaries.Sort([](const FPRProfileSummary& A, const FPRProfileSummary& B)
	{
		return A.LastSavedUtc > B.LastSavedUtc;
	});
	return Summaries;
}

FPRProfileOperationResult UPRSaveSubsystem::ActivateAndLoadProfile(const FGuid ProfileId)
{
	if (!SaveStore || !Catalog || !ProfileId.IsValid())
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Profile storage or ProfileId is invalid."), ProfileId);
		SetLastResult(Result);
		return Result;
	}

	UPRProfileSave* LoadedProfile = nullptr;
	FPRProfileOperationResult Result = SaveStore->LoadProfile(ProfileId, LoadedProfile);
	if (!Result.IsSuccess() || !LoadedProfile)
	{
		SetLastResult(Result);
		return Result;
	}
	const bool bRecovered = Result.bRecoveredFromBackup;
	const FPRProfileOperationResult MigrationResult = LoadedProfile->MigrateToLatest();
	if (!MigrationResult.IsSuccess())
	{
		SetLastResult(MigrationResult);
		return MigrationResult;
	}
	if (MigrationResult.bMigrated)
	{
		const FPRProfileOperationResult SaveResult = SaveStore->SaveProfile(LoadedProfile);
		if (!SaveResult.IsSuccess())
		{
			SetLastResult(SaveResult);
			return SaveResult;
		}
	}

	UPRProfileCatalogSave* CandidateCatalog = DuplicateObject<UPRProfileCatalogSave>(Catalog, this);
	CandidateCatalog->ProfileIds.AddUnique(ProfileId);
	CandidateCatalog->ActiveProfileId = ProfileId;
	const FPRProfileOperationResult CatalogResult = SaveStore->SaveCatalog(CandidateCatalog);
	if (!CatalogResult.IsSuccess())
	{
		SetLastResult(CatalogResult);
		return CatalogResult;
	}
	Catalog = CandidateCatalog;
	ActiveProfile = LoadedProfile;
	bHasAppliedActiveProfile = false;
	Result = FPRProfileOperationResult::MakeSuccess(ProfileId);
	Result.LoadedVersion = MigrationResult.LoadedVersion;
	Result.bMigrated = MigrationResult.bMigrated;
	Result.bRecoveredFromBackup = bRecovered;
	if (bRecovered)
	{
		Result.Diagnostic = TEXT("Primary profile was corrupt; the previous validated backup was restored.");
	}
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::DeleteProfile(const FGuid ProfileId)
{
	if (!SaveStore || !Catalog || !Catalog->ProfileIds.Contains(ProfileId))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NotFound, TEXT("Profile is not present in the catalog."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
	UPRProfileCatalogSave* CandidateCatalog = DuplicateObject<UPRProfileCatalogSave>(Catalog, this);
	CandidateCatalog->ProfileIds.Remove(ProfileId);
	if (CandidateCatalog->ActiveProfileId == ProfileId)
	{
		CandidateCatalog->ActiveProfileId.Invalidate();
	}
	FPRProfileOperationResult Result = SaveStore->SaveCatalog(CandidateCatalog);
	if (!Result.IsSuccess())
	{
		SetLastResult(Result);
		return Result;
	}
	Result = SaveStore->DeleteProfile(ProfileId);
	if (!Result.IsSuccess())
	{
		const FPRProfileOperationResult RollbackResult = SaveStore->SaveCatalog(Catalog);
		if (!RollbackResult.IsSuccess())
		{
			Catalog = CandidateCatalog;
			if (ActiveProfile && ActiveProfile->ProfileId == ProfileId)
			{
				ActiveProfile = nullptr;
				bHasAppliedActiveProfile = false;
			}
		}
		SetLastResult(Result);
		return Result;
	}
	Catalog = CandidateCatalog;
	if (ActiveProfile && ActiveProfile->ProfileId == ProfileId)
	{
		ActiveProfile = nullptr;
		bHasAppliedActiveProfile = false;
	}
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::SaveActiveProfile(const EPRSaveReason Reason)
{
	if (!SaveStore || !ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	FPRProfileOperationResult Result = SaveStore->SaveProfile(ActiveProfile);
	if (Result.IsSuccess())
	{
		UE_LOG(LogProjectA, Log, TEXT("Profile saved. ProfileId=%s Reason=%d"), *ActiveProfile->ProfileId.ToString(), static_cast<int32>(Reason));
	}
	SetLastResult(Result);
	return Result;
}

bool UPRSaveSubsystem::GetActiveProfileSnapshot(FPRProfileSnapshot& OutSnapshot) const
{
	if (!ActiveProfile)
	{
		return false;
	}
	OutSnapshot = ActiveProfile->Snapshot;
	return true;
}

FPRProfileOperationResult UPRSaveSubsystem::ReplaceActiveProfileSnapshot(const FPRProfileSnapshot& Snapshot)
{
	if (!ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	FString Diagnostic;
	if (!Snapshot.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	ActiveProfile->Snapshot = Snapshot;
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(ActiveProfile->ProfileId);
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::ApplyActiveProfileToPlayerState(APRPlayerState* PlayerState)
{
	if (!ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	FString Diagnostic;
	if (!PlayerState || !PlayerState->HasAuthority()
		|| !FPRProfileRuntimeBridge::ApplyToPlayerState(ActiveProfile->Snapshot, PlayerState, Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic.IsEmpty() ? TEXT("Profile can only be applied to an authoritative PlayerState.") : Diagnostic, ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	bHasAppliedActiveProfile = true;
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(ActiveProfile->ProfileId);
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::CaptureAndSaveAtSafeCheckpoint(APRPlayerState* PlayerState)
{
	if (!ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	if (!PlayerState || !PlayerState->HasAuthority() || IsUnsafeRiftCaptureContext(PlayerState))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::UnsafeCaptureContext, TEXT("Runtime profile capture is allowed only from an authoritative non-rift checkpoint."), ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	FPRProfileSnapshot Candidate = ActiveProfile->Snapshot;
	FString Diagnostic;
	if (!FPRProfileRuntimeBridge::CaptureFromPlayerState(PlayerState, Candidate, Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	ActiveProfile->Snapshot = MoveTemp(Candidate);
	return SaveActiveProfile(EPRSaveReason::SafeCheckpoint);
}

FPRProfileOperationResult UPRSaveSubsystem::CreateLegacyV1ProfileForDevelopment()
{
#if UE_BUILD_SHIPPING
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Legacy sample creation is disabled in Shipping."));
	SetLastResult(Result);
	return Result;
#else
	if (!SaveStore || !Catalog)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Profile storage is not initialized."));
		SetLastResult(Result);
		return Result;
	}
	UPRProfileSave* Legacy = NewObject<UPRProfileSave>(this);
	Legacy->ProfileId = FGuid::NewGuid();
	Legacy->DisplayName = TEXT("Legacy v1 Sample");
	Legacy->SaveVersion = 1;
	Legacy->CreatedUtc = FDateTime::UtcNow();
	Legacy->LastSavedUtc = Legacy->CreatedUtc;
	Legacy->Snapshot.ResourceWallet.Emplace(TEXT("EnergyCrystal"), 25);
	Legacy->Snapshot.SelectedRoleId = TEXT("Ability.Role.Assault");
	Legacy->Snapshot.UnlockedRoleIds.Add(Legacy->Snapshot.SelectedRoleId);
	FPRProfileOperationResult Result = SaveStore->SaveProfile(Legacy);
	if (Result.IsSuccess())
	{
		UPRProfileCatalogSave* CandidateCatalog = DuplicateObject<UPRProfileCatalogSave>(Catalog, this);
		CandidateCatalog->ProfileIds.AddUnique(Legacy->ProfileId);
		CandidateCatalog->ActiveProfileId = Legacy->ProfileId;
		Result = SaveStore->SaveCatalog(CandidateCatalog);
		if (Result.IsSuccess())
		{
			Catalog = CandidateCatalog;
			ActiveProfile = Legacy;
			bHasAppliedActiveProfile = false;
			Result.ProfileId = Legacy->ProfileId;
			Result.LoadedVersion = 1;
		}
		else
		{
			SaveStore->DeleteProfile(Legacy->ProfileId);
		}
	}
	SetLastResult(Result);
	return Result;
#endif
}

FPRProfileOperationResult UPRSaveSubsystem::CorruptActivePrimaryForDevelopment()
{
#if UE_BUILD_SHIPPING
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Corruption testing is disabled in Shipping."));
	SetLastResult(Result);
	return Result;
#else
	if (!SaveStore || !ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	const FGuid ProfileId = ActiveProfile->ProfileId;
	if (!IFileManager::Get().FileExists(*SaveStore->GetProfileBackupPath(ProfileId)))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Create at least two successful saves before testing backup recovery."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SaveStore->GetProfilePrimaryPath(ProfileId)) || Bytes.IsEmpty())
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not read the active primary profile."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
	Bytes.SetNum(FMath::Max(1, Bytes.Num() / 2));
	if (!FFileHelper::SaveArrayToFile(Bytes, *SaveStore->GetProfilePrimaryPath(ProfileId)))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Could not truncate the active primary profile."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
	return ActivateAndLoadProfile(ProfileId);
#endif
}

void UPRSaveSubsystem::HandleLocalLobbyPlayerReady(APlayerController* PlayerController)
{
	if (!bHasAppliedActiveProfile && ActiveProfile && PlayerController && PlayerController->IsLocalController() && PlayerController->HasAuthority())
	{
		ApplyActiveProfileToPlayerState(PlayerController->GetPlayerState<APRPlayerState>());
	}
}

FPRProfileOperationResult UPRSaveSubsystem::SaveForApplicationExit()
{
	if (!ActiveProfile)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && Cast<APRShipLobbyGameMode>(World->GetAuthGameMode()))
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (Controller->IsLocalController())
			{
				return CaptureAndSaveAtSafeCheckpoint(Controller->GetPlayerState<APRPlayerState>());
			}
		}
	}
	return SaveActiveProfile(EPRSaveReason::ApplicationExit);
}

void UPRSaveSubsystem::LoadOrRebuildCatalog()
{
	if (!SaveStore)
	{
		return;
	}
	UPRProfileCatalogSave* WorkingCatalog = nullptr;
	FPRProfileOperationResult Result = SaveStore->LoadCatalog(WorkingCatalog);
	if (Result.Status == EPRProfileOperationStatus::UnsupportedFutureVersion)
	{
		Catalog = nullptr;
		SetLastResult(Result);
		return;
	}
	if (!Result.IsSuccess() && Result.Status != EPRProfileOperationStatus::NotFound && Result.Status != EPRProfileOperationStatus::Corrupt)
	{
		Catalog = nullptr;
		SetLastResult(Result);
		return;
	}
	if (!Result.IsSuccess() || !WorkingCatalog || WorkingCatalog->CatalogVersion != UPRProfileCatalogSave::LatestCatalogVersion)
	{
		WorkingCatalog = NewObject<UPRProfileCatalogSave>(this);
		WorkingCatalog->ProfileIds = SaveStore->DiscoverProfileIds();
		WorkingCatalog->ActiveProfileId.Invalidate();
		Result = SaveStore->SaveCatalog(WorkingCatalog);
	}
	if (Result.IsSuccess())
	{
		Catalog = WorkingCatalog;
		Result = ReconcileCatalogWithDisk();
	}
	else
	{
		Catalog = nullptr;
	}
	SetLastResult(Result);
}

FPRProfileOperationResult UPRSaveSubsystem::ReconcileCatalogWithDisk()
{
	if (!Catalog || !SaveStore)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Profile catalog is not initialized."));
	}
	UPRProfileCatalogSave* CandidateCatalog = DuplicateObject<UPRProfileCatalogSave>(Catalog, this);
	bool bChanged = false;
	for (const FGuid& DiscoveredId : SaveStore->DiscoverProfileIds())
	{
		if (!CandidateCatalog->ProfileIds.Contains(DiscoveredId))
		{
			CandidateCatalog->ProfileIds.Add(DiscoveredId);
			bChanged = true;
		}
	}
	TSet<FGuid> SeenProfileIds;
	bChanged |= CandidateCatalog->ProfileIds.RemoveAll([this, &SeenProfileIds](const FGuid& Id)
	{
		if (!Id.IsValid() || SeenProfileIds.Contains(Id))
		{
			return true;
		}
		SeenProfileIds.Add(Id);
		UPRProfileSave* Profile = nullptr;
		const FPRProfileOperationResult Result = SaveStore->LoadProfile(Id, Profile);
		return Result.Status == EPRProfileOperationStatus::NotFound
			|| Result.Status == EPRProfileOperationStatus::Corrupt
			|| Result.Status == EPRProfileOperationStatus::ValidationFailed;
	}) > 0;
	if (CandidateCatalog->ActiveProfileId.IsValid() && !CandidateCatalog->ProfileIds.Contains(CandidateCatalog->ActiveProfileId))
	{
		CandidateCatalog->ActiveProfileId.Invalidate();
		bChanged = true;
	}
	if (!bChanged)
	{
		return FPRProfileOperationResult::MakeSuccess();
	}
	const FPRProfileOperationResult Result = SaveStore->SaveCatalog(CandidateCatalog);
	if (Result.IsSuccess())
	{
		Catalog = CandidateCatalog;
	}
	return Result;
}

bool UPRSaveSubsystem::IsUnsafeRiftCaptureContext(const APRPlayerState* PlayerState) const
{
	const UWorld* World = PlayerState ? PlayerState->GetWorld() : nullptr;
	return World && Cast<APRRiftGameMode>(World->GetAuthGameMode());
}

void UPRSaveSubsystem::SetLastResult(const FPRProfileOperationResult& Result)
{
	LastOperationResult = Result;
	if (!Result.IsSuccess())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Profile operation failed. Status=%d ProfileId=%s Diagnostic=%s"), static_cast<int32>(Result.Status), *Result.ProfileId.ToString(), *Result.Diagnostic);
	}
}
