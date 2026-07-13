#include "Core/PRShipLobbyGameMode.h"

#include "Diagnostics/PRDiagnosticsLog.h"

#include "Engine/World.h"
#include "Core/PRAssetManager.h"
#include "Core/PRGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/PRPlayerState.h"
#include "Persistence/PRSaveSubsystem.h"
#include "ProjectA.h"
#include "Progression/PRMissionProgressionDataAsset.h"

APRShipLobbyGameMode::APRShipLobbyGameMode()
	: RiftTestMapPath(TEXT("/Game/ProjectRift/Maps/L_Rift_Test"))
{
}

FString APRShipLobbyGameMode::BuildRiftTravelURL() const
{
	FString MapPath = RiftTestMapPath;
	FName MissionId = DefaultMissionId;
	if (const UPRMissionProgressionDataAsset* Mission = ResolveSelectedMission())
	{
		MissionId = Mission->MissionId;
		const FString AssetMapPath = Mission->MissionMap.ToSoftObjectPath().GetLongPackageName();
		if (!AssetMapPath.IsEmpty())
		{
			MapPath = AssetMapPath;
		}
	}
	return FString::Printf(TEXT("%s?listen?MissionId=%s"), *MapPath, *MissionId.ToString());
}

bool APRShipLobbyGameMode::ArePlayerStatesReadyForTravel(const TArray<APlayerState*>& PlayerStates) const
{
	if (PlayerStates.IsEmpty())
	{
		return false;
	}

	for (const APlayerState* PlayerState : PlayerStates)
	{
		const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
		if (!ProjectRiftPlayerState || !ProjectRiftPlayerState->IsMultiplayerProfileBound()
			|| ProjectRiftPlayerState->IsRepairPersistencePending() || !ProjectRiftPlayerState->IsReady())
		{
			return false;
		}
	}

	return true;
}

bool APRShipLobbyGameMode::CanStartRiftMission() const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return false;
	}

	TArray<APlayerState*> PlayerStates;
	PlayerStates.Reserve(CurrentGameState->PlayerArray.Num());
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		PlayerStates.Add(PlayerState.Get());
	}

	if (!ArePlayerStatesReadyForTravel(PlayerStates))
	{
		return false;
	}
	const UPRMissionProgressionDataAsset* Mission = ResolveSelectedMission();
	const APlayerController* HostController = FindHostPlayerController();
	const APRPlayerState* HostPlayerState = HostController ? HostController->GetPlayerState<APRPlayerState>() : nullptr;
	return Mission && HostPlayerState && Mission->IsEligible(HostPlayerState->GetBoundStoryProgress());
}

bool APRShipLobbyGameMode::StartRiftMission(APlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!IsHostPlayerController(RequestingController))
	{
		PRRecordDiagnosticEvent(this, EPRDiagnosticSeverity::Warning, TEXT("Flow"), TEXT("Lobby.StartRejected.NotHost"), TEXT("A non-host player attempted to start the rift mission."));
		if (RequestingController)
		{
			RequestingController->ClientMessage(TEXT("Only the listen-server host can start the rift mission."));
		}
		return false;
	}

	if (!CanStartRiftMission())
	{
		PRRecordDiagnosticEvent(this, EPRDiagnosticSeverity::Warning, TEXT("Flow"), TEXT("Lobby.StartRejected.NotReady"), TEXT("Mission start rejected because team readiness or host eligibility was incomplete."));
		if (RequestingController)
		{
			RequestingController->ClientMessage(TEXT("All players must be ready before starting the rift mission."));
		}
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString TravelURL = BuildRiftTravelURL();
	UE_LOG(LogProjectRiftFlow, Log, TEXT("Starting rift mission via ServerTravel: %s"), *TravelURL);
	const bool bTravelStarted = World->ServerTravel(TravelURL);
	PRRecordDiagnosticEvent(
		this,
		bTravelStarted ? EPRDiagnosticSeverity::Info : EPRDiagnosticSeverity::Error,
		TEXT("Flow"),
		bTravelStarted ? TEXT("Lobby.TravelStarted") : TEXT("Lobby.TravelFailed"),
		TravelURL);
	return bTravelStarted;
}

void APRShipLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (APRPlayerState* ProjectRiftPlayerState = NewPlayer ? NewPlayer->GetPlayerState<APRPlayerState>() : nullptr)
	{
		ProjectRiftPlayerState->SetReady(false);
	}
	if (NewPlayer && NewPlayer->IsLocalController())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UPRSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UPRSaveSubsystem>())
			{
				SaveSubsystem->HandleLocalLobbyPlayerReady(NewPlayer);
			}
		}
	}
	RefreshTeamMissionState();
}

void APRShipLobbyGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	RefreshTeamMissionState();
}

void APRShipLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	UPRMissionProgressionDataAsset* Mission = ResolveSelectedMission();
	if (APRGameState* ProjectRiftGameState = GetGameState<APRGameState>())
	{
		ProjectRiftGameState->SetTeamMissionState(
			Mission && Mission->IsContractValid() ? Mission->MissionId : DefaultMissionId,
			false);
	}
	RefreshTeamMissionState();
}

void APRShipLobbyGameMode::RefreshTeamMissionState()
{
	if (!HasAuthority())
	{
		return;
	}
	UPRMissionProgressionDataAsset* Mission = ResolveSelectedMission();
	if (APRGameState* ProjectRiftGameState = GetGameState<APRGameState>())
	{
		ProjectRiftGameState->SetTeamMissionState(
			Mission && Mission->IsContractValid() ? Mission->MissionId : DefaultMissionId,
			Mission && CanStartRiftMission());
	}
}

bool APRShipLobbyGameMode::IsHostPlayerController(const APlayerController* RequestingController) const
{
	return RequestingController && RequestingController->HasAuthority() && RequestingController->IsLocalController();
}

UPRMissionProgressionDataAsset* APRShipLobbyGameMode::ResolveSelectedMission() const
{
	FName MissionId = DefaultMissionId;
	if (const APRGameState* ProjectRiftGameState = GetGameState<APRGameState>())
	{
		if (!ProjectRiftGameState->GetSelectedTeamMissionId().IsNone())
		{
			MissionId = ProjectRiftGameState->GetSelectedTeamMissionId();
		}
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	return AssetManager ? AssetManager->LoadMissionSync(MissionId) : nullptr;
}

APlayerController* APRShipLobbyGameMode::FindHostPlayerController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* Controller = Iterator->Get();
		if (IsHostPlayerController(Controller))
		{
			return Controller;
		}
	}
	return nullptr;
}
