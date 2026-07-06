#include "Core/PRShipLobbyGameMode.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"

APRShipLobbyGameMode::APRShipLobbyGameMode()
	: RiftTestMapPath(TEXT("/Game/ProjectRift/Maps/L_Rift_Test"))
{
}

FString APRShipLobbyGameMode::BuildRiftTravelURL() const
{
	return RiftTestMapPath + TEXT("?listen");
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
		if (!ProjectRiftPlayerState || !ProjectRiftPlayerState->IsReady())
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

	return ArePlayerStatesReadyForTravel(PlayerStates);
}

bool APRShipLobbyGameMode::StartRiftMission(APlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!IsHostPlayerController(RequestingController))
	{
		if (RequestingController)
		{
			RequestingController->ClientMessage(TEXT("Only the listen-server host can start the rift mission."));
		}
		return false;
	}

	if (!CanStartRiftMission())
	{
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
	UE_LOG(LogProjectA, Log, TEXT("Starting rift mission via ServerTravel: %s"), *TravelURL);
	return World->ServerTravel(TravelURL);
}

bool APRShipLobbyGameMode::IsHostPlayerController(const APlayerController* RequestingController) const
{
	return RequestingController && RequestingController->HasAuthority() && RequestingController->IsLocalController();
}
