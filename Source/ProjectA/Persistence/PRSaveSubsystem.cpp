#include "Persistence/PRSaveSubsystem.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRAssetManager.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Diagnostics/PRDiagnosticsLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Persistence/PRProfileRuntimeBridge.h"
#include "Player/PRPlayerState.h"
#include "Player/PRPlayerController.h"
#include "ProjectA.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Ship/PRShipRepairDataAsset.h"

namespace ProjectRiftSaveSubsystemPrivate
{
bool AreResourceWalletsEqual(
	const TArray<FPRProfileResourceBalance>& A,
	const TArray<FPRProfileResourceBalance>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index].ResourceId != B[Index].ResourceId || A[Index].Count != B[Index].Count)
		{
			return false;
		}
	}
	return true;
}

bool AreShipModulesEqual(
	const TArray<FPRProfileShipModuleState>& A,
	const TArray<FPRProfileShipModuleState>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index].ModuleId != B[Index].ModuleId
			|| A[Index].Level != B[Index].Level
			|| A[Index].bUnlocked != B[Index].bUnlocked)
		{
			return false;
		}
	}
	return true;
}

bool AreStoriesEqual(const FPRProfileStoryProgress& A, const FPRProfileStoryProgress& B)
{
	return A.CurrentChapterId == B.CurrentChapterId
		&& A.UnlockedChapterIds == B.UnlockedChapterIds
		&& A.CompletedStoryNodeIds == B.CompletedStoryNodeIds;
}

bool DoesReceiptMatchExpectedState(
	const FPRShipRepairReceipt& Receipt,
	const FPRProfileSnapshot& Expected)
{
	FPRProfileSnapshot ReceiptState;
	ReceiptState.ResourceWallet = Receipt.SettledResourceWallet;
	ReceiptState.ShipModules = Receipt.SettledShipModules;
	ReceiptState.Story = Receipt.SettledStory;
	ReceiptState.Normalize();
	return AreResourceWalletsEqual(ReceiptState.ResourceWallet, Expected.ResourceWallet)
		&& AreShipModulesEqual(ReceiptState.ShipModules, Expected.ShipModules)
		&& AreStoriesEqual(ReceiptState.Story, Expected.Story);
}
}

bool UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
	const FString& RunIdText,
	const FString& CustomUserDirectory,
	const FString& ProjectAutomationDirectory,
	FString& OutProfileRoot,
	FString& OutDiagnostic)
{
	OutProfileRoot.Reset();
	OutDiagnostic.Reset();

	FGuid RunId;
	if (!FGuid::Parse(RunIdText, RunId) || !RunId.IsValid())
	{
		OutDiagnostic = TEXT("ProjectRiftGauntletSmoke must contain a valid GUID.");
		return false;
	}
	if (CustomUserDirectory.TrimStartAndEnd().IsEmpty())
	{
		OutDiagnostic = TEXT("Gauntlet smoke requires an explicit -userdir sandbox.");
		return false;
	}
	if (FPaths::IsRelative(CustomUserDirectory))
	{
		OutDiagnostic = TEXT("Gauntlet smoke requires an absolute -userdir sandbox.");
		return false;
	}
	if (ProjectAutomationDirectory.TrimStartAndEnd().IsEmpty())
	{
		OutDiagnostic = TEXT("ProjectA Automation directory is unavailable.");
		return false;
	}
	if (FPaths::IsRelative(ProjectAutomationDirectory))
	{
		OutDiagnostic = TEXT("ProjectA Automation directory must be absolute.");
		return false;
	}

	const FString AutomationRoot = FPaths::ConvertRelativePathToFull(ProjectAutomationDirectory);
	const FString ProjectSavedRoot = FPaths::GetPath(AutomationRoot);
	if (!FPaths::GetCleanFilename(AutomationRoot).Equals(TEXT("Automation"), ESearchCase::IgnoreCase)
		|| !FPaths::GetCleanFilename(ProjectSavedRoot).Equals(TEXT("Saved"), ESearchCase::IgnoreCase))
	{
		OutDiagnostic = TEXT("ProjectA Automation directory must end in Saved/Automation.");
		return false;
	}
	const FString RunRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		AutomationRoot,
		TEXT("Gauntlet"),
		RunId.ToString(EGuidFormats::DigitsWithHyphens)));
	const FString CustomUserRoot = FPaths::ConvertRelativePathToFull(CustomUserDirectory);
	FString NormalizedUserRoot = CustomUserRoot;
	FPaths::NormalizeDirectoryName(NormalizedUserRoot);
	if (NormalizedUserRoot.Len() == 2 && NormalizedUserRoot[1] == TEXT(':'))
	{
		OutDiagnostic = TEXT("Gauntlet -userdir cannot be a filesystem root.");
		return false;
	}
	if (FPaths::IsUnderDirectory(CustomUserRoot, ProjectSavedRoot)
		&& !FPaths::IsUnderDirectory(CustomUserRoot, AutomationRoot))
	{
		OutDiagnostic = TEXT("Gauntlet -userdir cannot access ProjectA non-automation saved data.");
		return false;
	}
	const FString CandidateRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(RunRoot, TEXT("Profiles")));
	if (!FPaths::IsUnderDirectory(CandidateRoot, AutomationRoot))
	{
		OutDiagnostic = TEXT("Resolved Gauntlet profile root escaped ProjectA Saved/Automation.");
		return false;
	}

	OutProfileRoot = CandidateRoot;
	OutDiagnostic = TEXT("Gauntlet profile root resolved.");
	return true;
}

void UPRSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bUsingIsolatedDevelopmentRoot = false;
	bGauntletSmokeInitializationRejected = false;
	FString SaveRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("ProjectRift"));
#if !UE_BUILD_SHIPPING
	FString RequestedGauntletRunId;
	if (FParse::Value(FCommandLine::Get(), TEXT("ProjectRiftGauntletSmoke="), RequestedGauntletRunId))
	{
		FString CustomUserDirectory;
		FParse::Value(FCommandLine::Get(), TEXT("userdir="), CustomUserDirectory);
		FString ProjectAutomationDirectory;
		FParse::Value(FCommandLine::Get(), TEXT("ProjectRiftGauntletAutomationRoot="), ProjectAutomationDirectory);
		FString Diagnostic;
		if (ResolveGauntletSmokeProfileRoot(
			RequestedGauntletRunId,
			CustomUserDirectory,
			ProjectAutomationDirectory,
			SaveRoot,
			Diagnostic))
		{
			bUsingIsolatedDevelopmentRoot = true;
			UE_LOG(LogProjectRiftSave, Log, TEXT("Using isolated Gauntlet profile root: %s"), *SaveRoot);
		}
		else
		{
			bGauntletSmokeInitializationRejected = true;
			UE_LOG(LogProjectRiftSave, Error, TEXT("Gauntlet profile initialization rejected: %s"), *Diagnostic);
		}
	}
	else
	{
		FString RequestedTestRoot;
		if (FParse::Value(FCommandLine::Get(), TEXT("ProjectRiftProfileRoot="), RequestedTestRoot))
		{
			const FString CandidateRoot = FPaths::ConvertRelativePathToFull(RequestedTestRoot);
			const FString AutomationRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation")));
			if (CandidateRoot == AutomationRoot || FPaths::IsUnderDirectory(CandidateRoot, AutomationRoot))
			{
				int32 PIEInstance = INDEX_NONE;
				UWorld* GameWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
				if (GEngine && GameWorld)
				{
					if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(GameWorld))
					{
						PIEInstance = WorldContext->PIEInstance;
					}
				}
				SaveRoot = PIEInstance == INDEX_NONE
					? CandidateRoot
					: FPaths::Combine(CandidateRoot, FString::Printf(TEXT("PIE_%d"), PIEInstance));
				bUsingIsolatedDevelopmentRoot = true;
				UE_LOG(LogProjectA, Log, TEXT("Using isolated ProjectRift development profile root: %s"), *SaveRoot);
			}
			else
			{
				UE_LOG(LogProjectA, Error, TEXT("Ignored ProjectRiftProfileRoot outside ProjectA Saved/Automation: %s"), *CandidateRoot);
			}
		}
	}
#endif
	if (bGauntletSmokeInitializationRejected)
	{
		SaveStore.Reset();
		Catalog = nullptr;
		ActiveProfile = nullptr;
		return;
	}
	SaveStore = MakeUnique<FPRSafeSaveStore>(SaveRoot);
	LoadOrRebuildCatalog();
	if (Catalog && Catalog->ActiveProfileId.IsValid())
	{
		ActivateAndLoadProfile(Catalog->ActiveProfileId);
	}
#if !UE_BUILD_SHIPPING
	else if (Catalog && FParse::Param(FCommandLine::Get(), TEXT("ProjectRiftCreateTestProfile")))
	{
		CreateProfile(TEXT("PIE Test Profile"));
	}
#endif
}

void UPRSaveSubsystem::Deinitialize()
{
	RetryPendingShipRepairReceipt();
	RetryPendingSettlementReceipt();
	ReleaseSessionProfileBinding();
	ActiveProfile = nullptr;
	Catalog = nullptr;
	SaveStore.Reset();
	bUsingIsolatedDevelopmentRoot = false;
	bGauntletSmokeInitializationRejected = false;
	Super::Deinitialize();
}

#if WITH_DEV_AUTOMATION_TESTS
void UPRSaveSubsystem::InitializeForAutomation(const FString& RootDirectory)
{
	ActiveProfile = nullptr;
	Catalog = nullptr;
	bHasAppliedActiveProfile = false;
	SessionBoundProfileId.Invalidate();
	bHasPendingSettlementReceipt = false;
	bHasPendingShipRepairReceipt = false;
	bGauntletSmokeInitializationRejected = false;
	const FString CandidateRoot = FPaths::ConvertRelativePathToFull(RootDirectory);
	const FString AutomationRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation")));
	bUsingIsolatedDevelopmentRoot = CandidateRoot == AutomationRoot || FPaths::IsUnderDirectory(CandidateRoot, AutomationRoot);
	if (!bUsingIsolatedDevelopmentRoot)
	{
		SaveStore.Reset();
		return;
	}
	SaveStore = MakeUnique<FPRSafeSaveStore>(CandidateRoot);
	LoadOrRebuildCatalog();
	if (Catalog && Catalog->ActiveProfileId.IsValid())
	{
		ActivateAndLoadProfile(Catalog->ActiveProfileId);
	}
}
#endif

FPRProfileOperationResult UPRSaveSubsystem::CreateProfile(const FString& DisplayName)
{
	if (IsSessionProfileBound())
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Busy, TEXT("Cannot create or switch profiles while a multiplayer session profile is bound."), SessionBoundProfileId);
		SetLastResult(Result);
		return Result;
	}
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
	if (IsSessionProfileBound() && ProfileId != SessionBoundProfileId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Busy, TEXT("Cannot switch profiles while a multiplayer session profile is bound."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
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
	if (IsSessionProfileBound() && ProfileId == SessionBoundProfileId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Busy, TEXT("Cannot delete the profile bound to the active multiplayer session."), ProfileId);
		SetLastResult(Result);
		return Result;
	}
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
	if (IsSessionProfileBound())
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Busy, TEXT("Cannot externally replace a profile snapshot while it is bound to a multiplayer session."), SessionBoundProfileId);
		SetLastResult(Result);
		return Result;
	}
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
	if (IsSessionProfileBound())
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::Busy,
			TEXT("Runtime checkpoint capture is locked while the active profile is bound to a multiplayer session."),
			SessionBoundProfileId);
		SetLastResult(Result);
		return Result;
	}
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

FPRProfileOperationResult UPRSaveSubsystem::BuildMultiplayerProfileProjection(FPRMultiplayerProfileProjection& OutProjection) const
{
	OutProjection = FPRMultiplayerProfileProjection();
	if (!ActiveProfile)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
	}

	OutProjection.ProfileId = ActiveProfile->ProfileId;
	OutProjection.DisplayName = ActiveProfile->DisplayName;
	OutProjection.BackpackItems = ActiveProfile->Snapshot.BackpackItems;
	OutProjection.ResourceWallet = ActiveProfile->Snapshot.ResourceWallet;
	OutProjection.SelectedRoleId = ActiveProfile->Snapshot.SelectedRoleId;
	OutProjection.Story = ActiveProfile->Snapshot.Story;
	OutProjection.ShipModules = ActiveProfile->Snapshot.ShipModules;
	FString Diagnostic;
	if (!OutProjection.IsValid(&Diagnostic))
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, ActiveProfile->ProfileId);
	}
	return FPRProfileOperationResult::MakeSuccess(ActiveProfile->ProfileId);
}

FPRProfileOperationResult UPRSaveSubsystem::BindActiveProfileToSession()
{
	FPRMultiplayerProfileProjection Projection;
	FPRProfileOperationResult Result = BuildMultiplayerProfileProjection(Projection);
	if (!Result.IsSuccess())
	{
		SetLastResult(Result);
		return Result;
	}
	if (IsSessionProfileBound() && SessionBoundProfileId != Projection.ProfileId)
	{
		Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::Busy, TEXT("A different profile is already bound to this multiplayer session."), SessionBoundProfileId);
		SetLastResult(Result);
		return Result;
	}
	SessionBoundProfileId = Projection.ProfileId;
	SetLastResult(Result);
	return Result;
}

void UPRSaveSubsystem::ReleaseSessionProfileBinding()
{
	SessionBoundProfileId.Invalidate();
}

FPRProfileOperationResult UPRSaveSubsystem::ApplyMultiplayerSettlementReceipt(const FPRPlayerSettlementReceipt& Receipt)
{
	FString Diagnostic;
	if (!Receipt.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	if (!SaveStore || !ActiveProfile || Receipt.ProfileId != ActiveProfile->ProfileId
		|| !IsSessionProfileBound() || Receipt.ProfileId != SessionBoundProfileId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Settlement receipt does not match the active session-bound profile."), Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	if (ActiveProfile->Snapshot.ProcessedSettlementIds.Contains(Receipt.SettlementId))
	{
		FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(Receipt.ProfileId);
		Result.bAlreadyProcessedSettlement = true;
		bHasPendingSettlementReceipt = false;
		SetLastResult(Result);
		return Result;
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRMissionProgressionDataAsset* Mission = AssetManager ? AssetManager->LoadMissionSync(Receipt.MissionId) : nullptr;
	if (!Mission || !Mission->IsContractValid(&Diagnostic) || Mission->MissionId != Receipt.MissionId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic.IsEmpty() ? TEXT("Settlement mission contract is unavailable or invalid.") : Diagnostic, Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}

	UPRProfileSave* Candidate = DuplicateObject<UPRProfileSave>(ActiveProfile, this);
	Candidate->Snapshot.BackpackItems = Receipt.SettledBackpackItems;
	Candidate->Snapshot.ResourceWallet = Receipt.SettledResourceWallet;
	Candidate->Snapshot.SelectedRoleId = Receipt.SettledRoleId;
	if (!Receipt.SettledRoleId.IsNone())
	{
		Candidate->Snapshot.UnlockedRoleIds.AddUnique(Receipt.SettledRoleId);
	}
	if (Receipt.bGrantStoryProgress)
	{
		if (!Mission->IsEligible(Candidate->Snapshot.Story))
		{
			const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("Local profile does not satisfy the mission prerequisites in the settlement receipt."), Receipt.ProfileId);
			SetLastResult(Result);
			return Result;
		}
		Mission->ApplyCompletion(Candidate->Snapshot.Story);
	}
	Candidate->Snapshot.ProcessedSettlementIds.Add(Receipt.SettlementId);
	Candidate->Snapshot.Normalize();
	if (!Candidate->Snapshot.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, Diagnostic, Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	Candidate->SaveVersion = UPRProfileSave::LatestSaveVersion;
	Candidate->LastSavedUtc = FDateTime::UtcNow();
	FPRProfileOperationResult Result = SaveStore->SaveProfile(Candidate);
	if (!Result.IsSuccess())
	{
		PendingSettlementReceipt = Receipt;
		bHasPendingSettlementReceipt = true;
		SetLastResult(Result);
		return Result;
	}
	ActiveProfile = Candidate;
	bHasPendingSettlementReceipt = false;
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::RetryPendingSettlementReceipt()
{
	if (!bHasPendingSettlementReceipt)
	{
		return FPRProfileOperationResult::MakeSuccess(GetActiveProfileId());
	}
	const FPRPlayerSettlementReceipt Receipt = PendingSettlementReceipt;
	return ApplyMultiplayerSettlementReceipt(Receipt);
}

FPRShipRepairEvaluation UPRSaveSubsystem::EvaluateShipRepair(const FName RepairProjectId) const
{
	FPRShipRepairEvaluation Evaluation;
	if (!ActiveProfile || RepairProjectId.IsNone())
	{
		Evaluation.Diagnostic = !ActiveProfile
			? TEXT("No active profile is loaded.")
			: TEXT("RepairProjectId is invalid.");
		return Evaluation;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRShipRepairDataAsset* Contract = AssetManager ? AssetManager->LoadShipRepairSync(RepairProjectId) : nullptr;
	TArray<UPRShipRepairDataAsset*> RepairCatalog;
	if (!Contract || !AssetManager->LoadShipRepairCatalog(RepairCatalog))
	{
		Evaluation.Diagnostic = TEXT("Ship repair contract or catalog is unavailable.");
		return Evaluation;
	}
	Evaluation = Contract->EvaluateRepair(ActiveProfile->Snapshot, RepairCatalog);
	return Evaluation;
}

FPRProfileOperationResult UPRSaveSubsystem::ApplyShipRepairReceipt(const FPRShipRepairReceipt& Receipt)
{
	FString Diagnostic;
	if (!Receipt.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			Diagnostic,
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	if (!SaveStore || !ActiveProfile || Receipt.ProfileId != ActiveProfile->ProfileId
		|| !IsSessionProfileBound() || Receipt.ProfileId != SessionBoundProfileId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			TEXT("Ship repair receipt does not match the active session-bound profile."),
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	if (bHasPendingShipRepairReceipt
		&& PendingShipRepairReceipt.TransactionId != Receipt.TransactionId)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::Busy,
			TEXT("A different ship repair receipt is still waiting to be saved."),
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	if (ActiveProfile->Snapshot.ProcessedRepairTransactionIds.Contains(Receipt.TransactionId))
	{
		FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(Receipt.ProfileId);
		Result.bAlreadyProcessedRepairTransaction = true;
		bHasPendingShipRepairReceipt = false;
		SetLastResult(Result);
		return Result;
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRShipRepairDataAsset* Contract = AssetManager ? AssetManager->LoadShipRepairSync(Receipt.RepairProjectId) : nullptr;
	TArray<UPRShipRepairDataAsset*> RepairCatalog;
	if (!Contract || Contract->RepairProjectId != Receipt.RepairProjectId
		|| !AssetManager->LoadShipRepairCatalog(RepairCatalog))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			TEXT("Ship repair receipt references an unavailable or invalid contract."),
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}

	FPRProfileSnapshot Expected = ActiveProfile->Snapshot;
	if (!Contract->ApplyRepairToSnapshot(Expected, RepairCatalog, &Diagnostic)
		|| !ProjectRiftSaveSubsystemPrivate::DoesReceiptMatchExpectedState(Receipt, Expected))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			Diagnostic.IsEmpty()
				? TEXT("Ship repair receipt absolute state does not match the locally calculated contract result.")
				: Diagnostic,
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}

	UPRProfileSave* Candidate = DuplicateObject<UPRProfileSave>(ActiveProfile, this);
	Candidate->Snapshot.ResourceWallet = Expected.ResourceWallet;
	Candidate->Snapshot.ShipModules = Expected.ShipModules;
	Candidate->Snapshot.Story = Expected.Story;
	Candidate->Snapshot.ProcessedRepairTransactionIds.Add(Receipt.TransactionId);
	Candidate->Snapshot.Normalize();
	if (!Candidate->Snapshot.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			Diagnostic,
			Receipt.ProfileId);
		SetLastResult(Result);
		return Result;
	}
	Candidate->SaveVersion = UPRProfileSave::LatestSaveVersion;
	Candidate->LastSavedUtc = FDateTime::UtcNow();
	FPRProfileOperationResult Result = SaveStore->SaveProfile(Candidate);
	if (!Result.IsSuccess())
	{
		PendingShipRepairReceipt = Receipt;
		bHasPendingShipRepairReceipt = true;
		SetLastResult(Result);
		return Result;
	}
	ActiveProfile = Candidate;
	bHasPendingShipRepairReceipt = false;
	SetLastResult(Result);
	return Result;
}

FPRProfileOperationResult UPRSaveSubsystem::RetryPendingShipRepairReceipt()
{
	return bHasPendingShipRepairReceipt
		? ApplyShipRepairReceipt(PendingShipRepairReceipt)
		: FPRProfileOperationResult::MakeSuccess(GetActiveProfileId());
}

FPRProfileOperationResult UPRSaveSubsystem::PrepareShipRepairAcceptanceForDevelopment()
{
#if UE_BUILD_SHIPPING
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
		EPRProfileOperationStatus::ValidationFailed,
		TEXT("Ship repair acceptance preparation is disabled in Shipping."),
		GetActiveProfileId());
	SetLastResult(Result);
	return Result;
#else
	if (!SaveStore || !ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::NoActiveProfile,
			TEXT("No active profile is loaded."));
		SetLastResult(Result);
		return Result;
	}
	if (bHasPendingShipRepairReceipt)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::Busy,
			TEXT("A ship repair receipt is still waiting to be saved."),
			ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRShipRepairDataAsset* Contract = AssetManager
		? AssetManager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage1"))
		: nullptr;
	FString Diagnostic;
	if (!Contract || !Contract->ValidateContract(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			Diagnostic.IsEmpty() ? TEXT("Acceptance repair contract is unavailable.") : Diagnostic,
			ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}

	UPRProfileSave* Candidate = DuplicateObject<UPRProfileSave>(ActiveProfile, this);
	for (const FName StoryNodeId : Contract->RequiredCompletedStoryNodeIds)
	{
		Candidate->Snapshot.Story.CompletedStoryNodeIds.AddUnique(StoryNodeId);
	}
	for (const FPRShipRepairResourceCost& Cost : Contract->ResourceCosts)
	{
		FPRProfileResourceBalance* Balance = Candidate->Snapshot.ResourceWallet.FindByPredicate([&Cost](const FPRProfileResourceBalance& Entry)
		{
			return Entry.ResourceId == Cost.ResourceId;
		});
		if (Balance)
		{
			Balance->Count = FMath::Max(Balance->Count, Cost.Count);
		}
		else
		{
			Candidate->Snapshot.ResourceWallet.Emplace(Cost.ResourceId, Cost.Count);
		}
	}
	Candidate->Snapshot.Normalize();
	if (!Candidate->Snapshot.IsValid(&Diagnostic))
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			Diagnostic,
			ActiveProfile->ProfileId);
		SetLastResult(Result);
		return Result;
	}
	Candidate->SaveVersion = UPRProfileSave::LatestSaveVersion;
	Candidate->LastSavedUtc = FDateTime::UtcNow();
	FPRProfileOperationResult Result = SaveStore->SaveProfile(Candidate);
	if (Result.IsSuccess())
	{
		ActiveProfile = Candidate;
	}
	SetLastResult(Result);
	return Result;
#endif
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
	if (!bUsingIsolatedDevelopmentRoot)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			TEXT("Primary corruption is allowed only under ProjectA Saved/Automation isolated profile roots."),
			GetActiveProfileId());
		SetLastResult(Result);
		return Result;
	}
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

FPRProfileOperationResult UPRSaveSubsystem::FailNextSaveForDevelopment()
{
#if UE_BUILD_SHIPPING
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
		EPRProfileOperationStatus::ValidationFailed,
		TEXT("Save failure injection is disabled in Shipping."),
		GetActiveProfileId());
	SetLastResult(Result);
	return Result;
#else
	if (!bUsingIsolatedDevelopmentRoot || !SaveStore || !ActiveProfile)
	{
		const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(
			EPRProfileOperationStatus::ValidationFailed,
			TEXT("Save failure injection requires an active profile under a ProjectA Saved/Automation isolated root."),
			GetActiveProfileId());
		SetLastResult(Result);
		return Result;
	}
	SaveStore->ArmNextSaveFailureForDevelopment();
	const FPRProfileOperationResult Result = FPRProfileOperationResult::MakeSuccess(GetActiveProfileId());
	SetLastResult(Result);
	return Result;
#endif
}

void UPRSaveSubsystem::HandleLocalLobbyPlayerReady(APlayerController* PlayerController)
{
	if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(PlayerController))
	{
		ProjectRiftController->SubmitLocalMultiplayerProfile();
		return;
	}
	if (!bHasAppliedActiveProfile && ActiveProfile && PlayerController && PlayerController->IsLocalController() && PlayerController->HasAuthority())
	{
		ApplyActiveProfileToPlayerState(PlayerController->GetPlayerState<APRPlayerState>());
	}
}

FPRProfileOperationResult UPRSaveSubsystem::SaveForApplicationExit()
{
	if (bHasPendingShipRepairReceipt)
	{
		return RetryPendingShipRepairReceipt();
	}
	if (bHasPendingSettlementReceipt)
	{
		const FPRProfileOperationResult PendingResult = RetryPendingSettlementReceipt();
		return PendingResult;
	}
	if (!ActiveProfile)
	{
		return FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::NoActiveProfile, TEXT("No active profile is loaded."));
	}
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!IsSessionProfileBound() && World && Cast<APRShipLobbyGameMode>(World->GetAuthGameMode()))
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
	FPRDiagnosticContext Context;
	Context.ProfileId = Result.ProfileId;
	const FString Message = Result.Diagnostic.IsEmpty()
		? FString::Printf(TEXT("Profile operation completed with status %d."), static_cast<int32>(Result.Status))
		: Result.Diagnostic;
	PRRecordDiagnosticEvent(
		GetGameInstance(),
		Result.IsSuccess() ? EPRDiagnosticSeverity::Info : EPRDiagnosticSeverity::Warning,
		TEXT("Save"),
		Result.IsSuccess() ? TEXT("Save.OperationSucceeded") : TEXT("Save.OperationFailed"),
		Message,
		Context);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogProjectRiftSave, Warning, TEXT("Profile operation failed. Status=%d ProfileId=%s Diagnostic=%s"), static_cast<int32>(Result.Status), *Result.ProfileId.ToString(), *Result.Diagnostic);
	}
}
