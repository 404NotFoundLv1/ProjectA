#include "Diagnostics/PRDeveloperToolsSubsystem.h"

#include "Diagnostics/PRDiagnosticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"

namespace
{
bool RequiresConfirmation(const EPRDeveloperAction Action)
{
	return Action == EPRDeveloperAction::DeleteProfile
		|| Action == EPRDeveloperAction::CorruptActivePrimary
		|| Action == EPRDeveloperAction::FailNextSave;
}

bool IsMap(const APlayerController* Controller, const TCHAR* MapToken)
{
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	return World && UWorld::RemovePIEPrefix(World->GetMapName()).Contains(MapToken, ESearchCase::IgnoreCase);
}

UPRSaveSubsystem* ResolveSaveSubsystem(const UPRDeveloperToolsSubsystem* DeveloperTools, const APlayerController* Controller)
{
	UGameInstance* GameInstance = Controller && Controller->GetWorld()
		? Controller->GetWorld()->GetGameInstance()
		: DeveloperTools->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UPRSaveSubsystem>() : nullptr;
}

FPRDeveloperActionResult FromProfileResult(const EPRDeveloperAction Action, const FPRProfileOperationResult& ProfileResult)
{
	FPRDeveloperActionResult Result;
	Result.Action = Action;
	Result.bSuccess = ProfileResult.IsSuccess();
	Result.Diagnostic = ProfileResult.Diagnostic.IsEmpty()
		? (Result.bSuccess ? TEXT("Developer action completed.") : TEXT("Developer action failed."))
		: ProfileResult.Diagnostic;
	return Result;
}
}

bool UPRDeveloperToolsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}

FPRDeveloperActionAvailability UPRDeveloperToolsSubsystem::GetActionAvailability(
	const EPRDeveloperAction Action,
	APlayerController* LocalController) const
{
	FPRDeveloperActionAvailability Availability;
	Availability.Action = Action;
	Availability.bRequiresConfirmation = RequiresConfirmation(Action);

	UPRSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem(this, LocalController);
	APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(LocalController);
	APRPlayerState* PlayerState = ProjectRiftController ? ProjectRiftController->GetPlayerState<APRPlayerState>() : nullptr;
	const bool bInLobby = IsMap(LocalController, TEXT("L_ShipLobby"));
	const bool bInRift = IsMap(LocalController, TEXT("L_Rift"));

	switch (Action)
	{
	case EPRDeveloperAction::CreateProfile:
	case EPRDeveloperAction::ActivateProfile:
	case EPRDeveloperAction::DeleteProfile:
	case EPRDeveloperAction::CreateLegacyV1Profile:
		Availability.bAvailable = SaveSubsystem && !SaveSubsystem->IsSessionProfileBound();
		Availability.DisabledReason = Availability.bAvailable ? FString() : TEXT("Profile storage is unavailable or locked by a multiplayer binding.");
		break;
	case EPRDeveloperAction::SaveCheckpoint:
		Availability.bAvailable = SaveSubsystem && SaveSubsystem->GetActiveProfileId().IsValid() && PlayerState && bInLobby;
		Availability.DisabledReason = Availability.bAvailable ? FString() : TEXT("A local lobby PlayerState and active profile are required.");
		break;
	case EPRDeveloperAction::PrepareShipRepairAcceptance:
		Availability.bAvailable = SaveSubsystem && SaveSubsystem->GetActiveProfileId().IsValid() && bInLobby;
		Availability.DisabledReason = Availability.bAvailable ? FString() : TEXT("An active profile in the ship lobby is required.");
		break;
	case EPRDeveloperAction::SpawnTestLoot:
		Availability.bAvailable = ProjectRiftController && bInRift;
		Availability.DisabledReason = Availability.bAvailable ? FString() : TEXT("Test loot can be requested only by a local rift controller.");
		break;
	case EPRDeveloperAction::ToggleReady:
	case EPRDeveloperAction::StartMission:
		Availability.bAvailable = ProjectRiftController && PlayerState && bInLobby;
		Availability.DisabledReason = Availability.bAvailable ? FString() : TEXT("This action is available only in the ship lobby.");
		break;
	case EPRDeveloperAction::CorruptActivePrimary:
	case EPRDeveloperAction::FailNextSave:
		Availability.bAvailable = SaveSubsystem
			&& SaveSubsystem->IsUsingIsolatedDevelopmentRoot()
			&& SaveSubsystem->GetActiveProfileId().IsValid();
		Availability.DisabledReason = Availability.bAvailable
			? FString()
			: TEXT("This fault action requires an active profile under ProjectA Saved/Automation.");
		break;
	default:
		Availability.DisabledReason = TEXT("Unknown developer action.");
		break;
	}
	return Availability;
}

FPRDeveloperActionResult UPRDeveloperToolsSubsystem::ExecuteAction(
	const FPRDeveloperActionRequest& Request,
	APlayerController* LocalController)
{
	FPRDeveloperActionResult Result;
	Result.Action = Request.Action;
	const FPRDeveloperActionAvailability Availability = GetActionAvailability(Request.Action, LocalController);
	if (!Availability.bAvailable)
	{
		Result.Diagnostic = Availability.DisabledReason;
		return Result;
	}
	if (Availability.bRequiresConfirmation && !Request.bConfirmed)
	{
		Result.Diagnostic = TEXT("Explicit confirmation is required for this destructive developer action.");
		return Result;
	}

	UPRSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem(this, LocalController);
	APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(LocalController);
	APRPlayerState* PlayerState = ProjectRiftController ? ProjectRiftController->GetPlayerState<APRPlayerState>() : nullptr;
	switch (Request.Action)
	{
	case EPRDeveloperAction::CreateProfile:
		Result = FromProfileResult(Request.Action, SaveSubsystem->CreateProfile(
			Request.TextValue.TrimStartAndEnd().IsEmpty() ? TEXT("Developer Profile") : Request.TextValue));
		break;
	case EPRDeveloperAction::ActivateProfile:
		Result = FromProfileResult(Request.Action, SaveSubsystem->ActivateAndLoadProfile(Request.ProfileId));
		break;
	case EPRDeveloperAction::DeleteProfile:
		Result = FromProfileResult(Request.Action, SaveSubsystem->DeleteProfile(Request.ProfileId));
		break;
	case EPRDeveloperAction::SaveCheckpoint:
		Result = FromProfileResult(Request.Action, SaveSubsystem->CaptureAndSaveAtSafeCheckpoint(PlayerState));
		break;
	case EPRDeveloperAction::CreateLegacyV1Profile:
		Result = FromProfileResult(Request.Action, SaveSubsystem->CreateLegacyV1ProfileForDevelopment());
		break;
	case EPRDeveloperAction::PrepareShipRepairAcceptance:
		Result = FromProfileResult(Request.Action, SaveSubsystem->PrepareShipRepairAcceptanceForDevelopment());
		break;
	case EPRDeveloperAction::SpawnTestLoot:
		ProjectRiftController->SpawnTestLoot();
		Result.bSuccess = true;
		Result.Diagnostic = TEXT("Test loot request sent to the authoritative server.");
		break;
	case EPRDeveloperAction::ToggleReady:
		ProjectRiftController->ToggleReady();
		Result.bSuccess = true;
		Result.Diagnostic = TEXT("Ready-state toggle request sent.");
		break;
	case EPRDeveloperAction::StartMission:
		ProjectRiftController->StartRiftMission();
		Result.bSuccess = true;
		Result.Diagnostic = TEXT("Mission-start request sent to the authoritative server.");
		break;
	case EPRDeveloperAction::CorruptActivePrimary:
		Result = FromProfileResult(Request.Action, SaveSubsystem->CorruptActivePrimaryForDevelopment());
		break;
	case EPRDeveloperAction::FailNextSave:
		Result = FromProfileResult(Request.Action, SaveSubsystem->FailNextSaveForDevelopment());
		break;
	default:
		Result.Diagnostic = TEXT("Unknown developer action.");
		break;
	}

	UGameInstance* GameInstance = LocalController && LocalController->GetWorld()
		? LocalController->GetWorld()->GetGameInstance()
		: GetGameInstance();
	if (GameInstance)
	{
		if (UPRDiagnosticsSubsystem* Diagnostics = GameInstance->GetSubsystem<UPRDiagnosticsSubsystem>())
		{
			FPRDiagnosticContext Context;
			Context.ProfileId = SaveSubsystem ? SaveSubsystem->GetActiveProfileId() : FGuid();
			Diagnostics->RecordEvent(
				Result.bSuccess ? EPRDiagnosticSeverity::Info : EPRDiagnosticSeverity::Warning,
				TEXT("Diagnostics"),
				Result.bSuccess ? TEXT("DeveloperAction.Succeeded") : TEXT("DeveloperAction.Failed"),
				Result.Diagnostic,
				Context);
		}
	}
	return Result;
}
