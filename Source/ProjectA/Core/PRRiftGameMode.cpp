#include "Core/PRRiftGameMode.h"

#include "Core/PRSpawnManager.h"
#include "Core/PRRiftObjectiveActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "ProjectA.h"

APRRiftGameMode::APRRiftGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = APRRiftGameState::StaticClass();
}

void APRRiftGameMode::BeginPlay()
{
	Super::BeginPlay();

	StartRiftMission();
}

void APRRiftGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bRiftMissionStarted)
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetMissionTime(RiftGameState->GetMissionTime() + DeltaSeconds);
	}
}

void APRRiftGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UpdateAlivePlayerCount();
}

void APRRiftGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	UpdateAlivePlayerCount();
}

bool APRRiftGameMode::StartRiftMission()
{
	if (!HasAuthority())
	{
		return false;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift mission start failed: APRRiftGameState is missing."));
		return false;
	}

	bRiftMissionStarted = true;
	ActiveObjective = nullptr;
	ResetExtractionState();
	StopSpawnManagers();
	RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::NotStarted);
	RiftGameState->SetObjectiveProgress(0.0f);
	RiftGameState->SetRiftStability(InitialRiftStability);
	RiftGameState->SetExtractionAvailable(false);
	RiftGameState->SetMissionTime(0.0f);
	UpdateAlivePlayerCount();

	UE_LOG(LogProjectA, Log, TEXT("Rift mission started. Stability=%.1f AlivePlayers=%d"),
		RiftGameState->GetRiftStability(),
		RiftGameState->GetAlivePlayerCount());

	return true;
}

void APRRiftGameMode::CompleteCurrentObjective()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		StopSpawnManagers();
		RiftGameState->SetObjectiveProgress(1.0f);
		RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::Completed);
		OpenExtraction();
	}
}

void APRRiftGameMode::OpenExtraction()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetExtractionAvailable(true);
		CheckExtractionCompletion();
	}
}

bool APRRiftGameMode::RegisterPlayerExtracted(AController* ExtractingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	APlayerState* ExtractingPlayerState = ResolveExtractionPlayerState(ExtractingController);
	if (!RiftGameState || !ExtractingPlayerState || !RiftGameState->IsExtractionAvailable() || RiftGameState->IsExtractionComplete())
	{
		return false;
	}

	const TObjectKey<APlayerState> PlayerKey(ExtractingPlayerState);
	if (ExtractedPlayerStates.Contains(PlayerKey))
	{
		return false;
	}

	ExtractedPlayerStates.Add(PlayerKey);
	RiftGameState->SetExtractedPlayerCount(ExtractedPlayerStates.Num());
	CheckExtractionCompletion();

	UE_LOG(LogProjectA, Log, TEXT("Rift player extracted: %s (%d/%d)"),
		*GetNameSafe(ExtractingPlayerState),
		RiftGameState->GetExtractedPlayerCount(),
		RiftGameState->GetAlivePlayerCount());

	return true;
}

bool APRRiftGameMode::IsPlayerExtracted(AController* Controller) const
{
	APlayerState* PlayerState = ResolveExtractionPlayerState(Controller);
	return PlayerState && ExtractedPlayerStates.Contains(TObjectKey<APlayerState>(PlayerState));
}

void APRRiftGameMode::ResetExtractionState()
{
	if (!HasAuthority())
	{
		return;
	}

	ExtractedPlayerStates.Reset();
	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetExtractedPlayerCount(0);
		RiftGameState->SetExtractionComplete(false);
	}

	bReturnToLobbyTravelPending = false;
}

void APRRiftGameMode::CheckExtractionCompletion()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		const int32 AlivePlayers = RiftGameState->GetAlivePlayerCount();
		const int32 ExtractedPlayers = RiftGameState->GetExtractedPlayerCount();
		const bool bWasExtractionComplete = RiftGameState->IsExtractionComplete();
		const bool bAllAlivePlayersExtracted = RiftGameState->IsExtractionAvailable()
			&& AlivePlayers > 0
			&& ExtractedPlayers >= AlivePlayers;

		RiftGameState->SetExtractionComplete(bAllAlivePlayersExtracted);
		if (bAllAlivePlayersExtracted && !bWasExtractionComplete)
		{
			RequestReturnToLobbyTravel();
		}
	}
}

FString APRRiftGameMode::BuildReturnToLobbyTravelURL() const
{
	if (ReturnToLobbyMapPath.IsEmpty())
	{
		return FString();
	}

	return ReturnToLobbyMapPath.Contains(TEXT("?"))
		? ReturnToLobbyMapPath
		: ReturnToLobbyMapPath + TEXT("?listen");
}

void APRRiftGameMode::SetReturnToLobbyServerTravelEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	bReturnToLobbyServerTravelEnabled = bEnabled;
}

void APRRiftGameMode::UpdateAlivePlayerCount()
{
	if (!HasAuthority())
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetAlivePlayerCount(CountAlivePlayers());
		CheckExtractionCompletion();
	}
}

void APRRiftGameMode::HandleObjectiveActivated(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority() || !ObjectiveActor)
	{
		return;
	}

	ActiveObjective = ObjectiveActor;
	StartSpawnManagersForObjective(ObjectiveActor);

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::Active);
		RiftGameState->SetObjectiveProgress(ObjectiveActor->GetObjectiveProgress());
	}
}

void APRRiftGameMode::HandleObjectiveProgressChanged(APRRiftObjectiveActor* ObjectiveActor, const float ObjectiveProgress)
{
	if (!HasAuthority() || !ObjectiveActor)
	{
		return;
	}

	if (ActiveObjective && ActiveObjective != ObjectiveActor)
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetObjectiveProgress(ObjectiveProgress);
	}
}

void APRRiftGameMode::HandleObjectiveCompleted(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority() || !ObjectiveActor)
	{
		return;
	}

	if (ActiveObjective && ActiveObjective != ObjectiveActor)
	{
		return;
	}

	ActiveObjective = ObjectiveActor;
	StopSpawnManagers();
	CompleteCurrentObjective();
}

bool APRRiftGameMode::StartSpawnManagersForObjective(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority())
	{
		return false;
	}

	DiscoverSpawnManagers();
	if (SpawnManagers.IsEmpty())
	{
		if (APRSpawnManager* FallbackSpawnManager = CreateFallbackSpawnManager(ObjectiveActor))
		{
			SpawnManagers.Add(FallbackSpawnManager);
		}
	}

	bool bStartedAny = false;
	for (const TObjectPtr<APRSpawnManager>& SpawnManager : SpawnManagers)
	{
		if (SpawnManager && SpawnManager->StartSpawningForObjective(ObjectiveActor))
		{
			bStartedAny = true;
		}
	}

	return bStartedAny;
}

void APRRiftGameMode::StopSpawnManagers()
{
	if (!HasAuthority())
	{
		return;
	}

	DiscoverSpawnManagers();
	for (const TObjectPtr<APRSpawnManager>& SpawnManager : SpawnManagers)
	{
		if (SpawnManager)
		{
			SpawnManager->StopSpawning();
		}
	}
}

APRRiftGameState* APRRiftGameMode::GetRiftGameState() const
{
	return GetGameState<APRRiftGameState>();
}

int32 APRRiftGameMode::CountAlivePlayers() const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return 0;
	}

	int32 AliveCount = 0;
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		if (PlayerState && !PlayerState->IsOnlyASpectator())
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

void APRRiftGameMode::DiscoverSpawnManagers()
{
	SpawnManagers.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APRSpawnManager> SpawnManagerIt(World); SpawnManagerIt; ++SpawnManagerIt)
	{
		if (APRSpawnManager* SpawnManager = *SpawnManagerIt)
		{
			SpawnManagers.Add(SpawnManager);
		}
	}
}

APRSpawnManager* APRRiftGameMode::CreateFallbackSpawnManager(APRRiftObjectiveActor* ObjectiveActor)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector SpawnLocation = ObjectiveActor ? ObjectiveActor->GetActorLocation() : FVector::ZeroVector;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<APRSpawnManager>(
		APRSpawnManager::StaticClass(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
}

APlayerState* APRRiftGameMode::ResolveExtractionPlayerState(AController* Controller) const
{
	return Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
}

void APRRiftGameMode::RequestReturnToLobbyTravel()
{
	if (!HasAuthority() || bReturnToLobbyTravelPending)
	{
		return;
	}

	const FString TravelURL = BuildReturnToLobbyTravelURL();
	if (TravelURL.IsEmpty())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift extraction completed but return lobby map path is empty."));
		return;
	}

	bReturnToLobbyTravelPending = true;
	StopSpawnManagers();

	if (!bReturnToLobbyServerTravelEnabled)
	{
		UE_LOG(LogProjectA, Log, TEXT("Rift extraction completed. Return travel recorded but disabled for this run: %s"), *TravelURL);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift extraction completed but world is missing for return travel: %s"), *TravelURL);
		return;
	}

	UE_LOG(LogProjectA, Log, TEXT("Rift extraction completed. Returning to lobby via ServerTravel: %s"), *TravelURL);
	if (!World->ServerTravel(TravelURL))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift return ServerTravel failed: %s"), *TravelURL);
	}
}
