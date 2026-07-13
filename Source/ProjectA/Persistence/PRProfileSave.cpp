#include "Persistence/PRProfileSave.h"

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
	return Snapshot.IsValid(OutDiagnostic);
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
