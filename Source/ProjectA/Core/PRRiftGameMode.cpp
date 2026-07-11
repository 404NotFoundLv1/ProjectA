#include "Core/PRRiftGameMode.h"

#include "Characters/PRCharacter.h"
#include "Core/PRSpawnManager.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Enemies/PREnemyCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerState.h"
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

void APRRiftGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LogFlowPhase(TEXT("EndPlay"));
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTravelTimerHandle);
	StopSpawnManagers();
	SpawnManagers.Reset();
	ActiveObjective = nullptr;
	ExtractedPlayerStates.Reset();
	CountedKilledEnemies.Reset();
	bRiftMissionStarted = false;
	bReturnToLobbyTravelPending = false;
	bSettlementFinalizationInProgress = false;

	Super::EndPlay(EndPlayReason);
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
		if (RiftGameState->IsSettlementReady())
		{
			return;
		}

		RiftGameState->SetMissionTime(RiftGameState->GetMissionTime() + DeltaSeconds);
		DrainRiftStability(DeltaSeconds);
		CheckFailureConditions();
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

	CurrentRunId = FGuid::NewGuid();
	CurrentSettlementId = FGuid::NewGuid();
	FinalizedRunId.Invalidate();
	bSettlementFinalizationInProgress = false;
	CountedKilledEnemies.Reset();
	bRiftMissionStarted = true;
	ActiveObjective = nullptr;
	ResetExtractionState();
	StopSpawnManagers();
	RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::NotStarted);
	RiftGameState->SetObjectiveProgress(0.0f);
	RiftGameState->SetRiftStability(InitialRiftStability);
	RiftGameState->SetExtractionAvailable(false);
	RiftGameState->SetMissionTime(0.0f);
	RiftGameState->SetKilledEnemyCount(0);
	UpdateAlivePlayerCount();

	UE_LOG(LogProjectA, Log, TEXT("Rift mission started. Stability=%.1f AlivePlayers=%d"),
		RiftGameState->GetRiftStability(),
		RiftGameState->GetAlivePlayerCount());
	LogFlowPhase(TEXT("MissionStarted"));

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
		LogFlowPhase(TEXT("ExtractionOpened"));
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
	LogFlowPhase(TEXT("PlayerExtracted"), ExtractingPlayerState);
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
		RiftGameState->ResetSettlementData();
	}

	bReturnToLobbyTravelPending = false;
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTravelTimerHandle);
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
			RequestReturnToLobbyTravel(EPRRiftMissionResult::Success);
		}
	}
}

bool APRRiftGameMode::RequestRiftFailure()
{
	if (!HasAuthority() || bReturnToLobbyTravelPending)
	{
		return false;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || RiftGameState->IsSettlementReady())
	{
		return false;
	}

	bRiftMissionStarted = false;
	RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::Failed);
	RiftGameState->SetExtractionAvailable(false);
	RiftGameState->SetExtractionComplete(false);
	StopSpawnManagers();
	LogFlowPhase(TEXT("MissionFailed"));
	RequestReturnToLobbyTravel(EPRRiftMissionResult::Failed);

	return RiftGameState->IsSettlementReady();
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

FPRRiftSettlementData APRRiftGameMode::GenerateSettlementData(const EPRRiftMissionResult Result) const
{
	FPRRiftSettlementData Settlement;
	Settlement.MissionId = MissionId;
	Settlement.RunId = CurrentRunId;
	Settlement.SettlementId = CurrentSettlementId;
	Settlement.Result = Result;

	if (const APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		Settlement.MissionTime = RiftGameState->GetMissionTime();
		Settlement.RiftStability = RiftGameState->GetRiftStability();
		Settlement.ObjectiveProgress = RiftGameState->GetObjectiveProgress();
		Settlement.AlivePlayerCount = RiftGameState->GetAlivePlayerCount();
		Settlement.ExtractedPlayerCount = RiftGameState->GetExtractedPlayerCount();
		Settlement.KilledEnemyCount = RiftGameState->GetKilledEnemyCount();
	}

	CountExtractedInventoryItems(Settlement.ExtractedItemCount, Settlement.ExtractedUniqueItemTypes);

	return Settlement;
}

void APRRiftGameMode::FinalizeRiftSettlement(const EPRRiftMissionResult Result)
{
	if (!HasAuthority() || !CurrentRunId.IsValid() || !CurrentSettlementId.IsValid())
	{
		return;
	}

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		if (bSettlementFinalizationInProgress || FinalizedRunId == CurrentRunId || RiftGameState->IsSettlementReady())
		{
			LogFlowPhase(TEXT("SettlementDuplicateIgnored"));
			return;
		}

		bSettlementFinalizationInProgress = true;
		FPRRiftSettlementData Settlement = GenerateSettlementData(Result);
		ApplyExtractedResourceRules(Result, Settlement);

		RiftGameState->SetSettlementData(Settlement);
		RiftGameState->SetSettlementReady(true);
		FinalizedRunId = CurrentRunId;
		bSettlementFinalizationInProgress = false;
		LogFlowPhase(TEXT("SettlementFinalized"));

		const FPRRiftSettlementData FinalSettlement = RiftGameState->GetSettlementData();
		UE_LOG(LogProjectA, Log, TEXT("Rift settlement finalized. Result=%d Time=%.1f Extracted=%d/%d Kills=%d Items=%d Types=%d Resources=%d ResourceTypes=%d LostResources=%d"),
			static_cast<int32>(FinalSettlement.Result),
			FinalSettlement.MissionTime,
			FinalSettlement.ExtractedPlayerCount,
			FinalSettlement.AlivePlayerCount,
			FinalSettlement.KilledEnemyCount,
			FinalSettlement.ExtractedItemCount,
			FinalSettlement.ExtractedUniqueItemTypes,
			FinalSettlement.ExtractedResourceCount,
			FinalSettlement.ExtractedUniqueResourceTypes,
			FinalSettlement.LostResourceCount);
	}
}

int32 APRRiftGameMode::CalculateRetainedResourceCount(const int32 Count, const EPRRiftMissionResult Result) const
{
	const int32 SafeCount = FMath::Max(0, Count);
	if (SafeCount <= 0)
	{
		return 0;
	}

	if (Result == EPRRiftMissionResult::Success)
	{
		return SafeCount;
	}

	if (Result == EPRRiftMissionResult::Failed)
	{
		return FMath::FloorToInt(SafeCount * FMath::Clamp(FailedResourceRetentionRate, 0.0f, 1.0f));
	}

	return 0;
}

bool APRRiftGameMode::IsExtractableResourceItem(const FPRItemInstance& Item, UPRInventoryComponent* InventoryComponent) const
{
	if (!Item.IsValid())
	{
		return false;
	}

	if (const UPRItemDataAsset* ItemData = InventoryComponent ? InventoryComponent->FindItemData(Item.ItemId) : nullptr)
	{
		return ItemData->ItemType == EPRItemType::Material;
	}

	return IsFallbackMaterialResourceId(Item.ItemId);
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
		CheckFailureConditions();
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
	LogFlowPhase(TEXT("ObjectiveActivated"));

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
	LogFlowPhase(TEXT("ObjectiveCompleted"));
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

bool APRRiftGameMode::RegisterEnemyKilled(APREnemyCharacter* Enemy, AController* Killer)
{
	if (!HasAuthority() || !Enemy)
	{
		return false;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || RiftGameState->IsSettlementReady())
	{
		return false;
	}

	const TObjectKey<APREnemyCharacter> EnemyKey(Enemy);
	if (CountedKilledEnemies.Contains(EnemyKey))
	{
		return false;
	}
	CountedKilledEnemies.Add(EnemyKey);

	RiftGameState->IncrementKilledEnemyCount();
	UE_LOG(LogProjectA, Log, TEXT("Rift enemy kill registered. Enemy=%s Killer=%s Kills=%d"),
		*GetNameSafe(Enemy),
		*GetNameSafe(Killer),
		RiftGameState->GetKilledEnemyCount());
	return true;
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
		if (PlayerState && !PlayerState->IsOnlyASpectator() && IsPlayerStateAliveForRift(PlayerState.Get()))
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

bool APRRiftGameMode::CheckFailureConditions()
{
	if (!HasAuthority() || !bRiftMissionStarted || bReturnToLobbyTravelPending)
	{
		return false;
	}

	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || RiftGameState->IsSettlementReady())
	{
		return false;
	}

	if (RiftGameState->GetRiftStability() <= 0.0f)
	{
		return RequestRiftFailure();
	}

	if (AreAllActivePlayersDefeated())
	{
		return RequestRiftFailure();
	}

	return false;
}

bool APRRiftGameMode::AreAllActivePlayersDefeated() const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return false;
	}

	bool bFoundEligiblePlayer = false;
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		APlayerState* RawPlayerState = PlayerState.Get();
		if (!RawPlayerState || RawPlayerState->IsOnlyASpectator() || ExtractedPlayerStates.Contains(TObjectKey<APlayerState>(RawPlayerState)))
		{
			continue;
		}

		bFoundEligiblePlayer = true;
		if (IsPlayerStateAliveForRift(RawPlayerState))
		{
			return false;
		}
	}

	return bFoundEligiblePlayer;
}

bool APRRiftGameMode::IsPlayerStateAliveForRift(const APlayerState* PlayerState) const
{
	if (!PlayerState || PlayerState->IsOnlyASpectator())
	{
		return false;
	}

	const APawn* Pawn = ResolvePawnForPlayerState(PlayerState);
	const APRCharacter* ProjectRiftCharacter = Cast<APRCharacter>(Pawn);
	if (ProjectRiftCharacter && !ProjectRiftCharacter->IsAbilitySystemInitialized())
	{
		return true;
	}

	return ProjectRiftCharacter ? ProjectRiftCharacter->IsAlive() : true;
}

APawn* APRRiftGameMode::ResolvePawnForPlayerState(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	if (const AController* OwnerController = Cast<AController>(PlayerState->GetOwner()))
	{
		if (OwnerController->GetPlayerState<APlayerState>() == PlayerState)
		{
			return OwnerController->GetPawn();
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AController> ControllerIt(World); ControllerIt; ++ControllerIt)
	{
		AController* Controller = *ControllerIt;
		if (Controller && Controller->GetPlayerState<APlayerState>() == PlayerState)
		{
			return Controller->GetPawn();
		}
	}

	return nullptr;
}

void APRRiftGameMode::DrainRiftStability(const float DeltaSeconds)
{
	if (RiftStabilityDrainPerSecond <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || RiftGameState->GetCurrentObjectiveState() != EPRRiftObjectiveState::Active)
	{
		return;
	}

	RiftGameState->SetRiftStability(RiftGameState->GetRiftStability() - RiftStabilityDrainPerSecond * DeltaSeconds);
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

void APRRiftGameMode::CountExtractedInventoryItems(int32& OutItemCount, int32& OutUniqueItemTypes) const
{
	OutItemCount = 0;
	OutUniqueItemTypes = 0;

	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return;
	}

	TSet<FName> UniqueItemIds;
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		if (!PlayerState || !ExtractedPlayerStates.Contains(TObjectKey<APlayerState>(PlayerState.Get())))
		{
			continue;
		}

		const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState.Get());
		const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
		if (!InventoryComponent)
		{
			continue;
		}

		for (const FPRItemInstance& Item : InventoryComponent->GetItems())
		{
			if (!Item.IsValid())
			{
				continue;
			}

			OutItemCount += Item.Count;
			UniqueItemIds.Add(Item.ItemId);
		}
	}

	OutUniqueItemTypes = UniqueItemIds.Num();
}

void APRRiftGameMode::ApplyExtractedResourceRules(const EPRRiftMissionResult Result, FPRRiftSettlementData& InOutSettlementData)
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return;
	}

	TSet<FName> UniqueResourceIds;
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		if (!ShouldApplyResourceRulesToPlayer(PlayerState.Get(), Result))
		{
			continue;
		}

		ApplyExtractedResourceRulesForPlayer(
			Cast<APRPlayerState>(PlayerState.Get()),
			Result,
			InOutSettlementData,
			UniqueResourceIds);
	}

	InOutSettlementData.ExtractedUniqueResourceTypes = UniqueResourceIds.Num();
}

void APRRiftGameMode::ApplyExtractedResourceRulesForPlayer(
	APRPlayerState* PlayerState,
	const EPRRiftMissionResult Result,
	FPRRiftSettlementData& InOutSettlementData,
	TSet<FName>& UniqueResourceIds) const
{
	UPRInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!PlayerState || !InventoryComponent)
	{
		return;
	}

	const TArray<FPRItemInstance> InventoryItems = InventoryComponent->GetItems();
	for (const FPRItemInstance& Item : InventoryItems)
	{
		if (!IsExtractableResourceItem(Item, InventoryComponent))
		{
			continue;
		}

		const int32 OriginalCount = FMath::Max(0, Item.Count);
		const int32 RetainedCount = CalculateRetainedResourceCount(OriginalCount, Result);
		const int32 LostCount = FMath::Max(0, OriginalCount - RetainedCount);

		if (RetainedCount > 0)
		{
			PlayerState->AddShipResource(Item.ItemId, RetainedCount);
			InOutSettlementData.ExtractedResourceCount += RetainedCount;
			UniqueResourceIds.Add(Item.ItemId);
		}

		InOutSettlementData.LostResourceCount += LostCount;

		if (!InventoryComponent->RemoveItem(Item.ItemId, OriginalCount))
		{
			UE_LOG(LogProjectA, Warning, TEXT("Rift resource extraction could not remove inventory item. Player=%s ItemId=%s Count=%d"),
				*GetNameSafe(PlayerState),
				*Item.ItemId.ToString(),
				OriginalCount);
		}
	}
}

bool APRRiftGameMode::ShouldApplyResourceRulesToPlayer(const APlayerState* PlayerState, const EPRRiftMissionResult Result) const
{
	if (!PlayerState || PlayerState->IsOnlyASpectator())
	{
		return false;
	}

	if (Result == EPRRiftMissionResult::Success)
	{
		return ExtractedPlayerStates.Contains(TObjectKey<APlayerState>(const_cast<APlayerState*>(PlayerState)));
	}

	return Result == EPRRiftMissionResult::Failed;
}

bool APRRiftGameMode::IsFallbackMaterialResourceId(const FName ItemId)
{
	return ItemId == FName(TEXT("RiftShard"))
		|| ItemId == FName(TEXT("EnergyCrystal"))
		|| ItemId == FName(TEXT("CommonChip"));
}

void APRRiftGameMode::RequestReturnToLobbyTravel(const EPRRiftMissionResult Result)
{
	if (!HasAuthority() || bReturnToLobbyTravelPending)
	{
		return;
	}

	FinalizeRiftSettlement(Result);
	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || !RiftGameState->IsSettlementReady())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift return travel aborted because settlement was not finalized."));
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
	LogFlowPhase(TEXT("ReturnScheduled"));

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

	if (ReturnToLobbyDelayAfterSettlement > 0.0f)
	{
		UE_LOG(LogProjectA, Log, TEXT("Rift extraction completed. Showing settlement for %.1f seconds before return travel: %s"),
			ReturnToLobbyDelayAfterSettlement,
			*TravelURL);
		World->GetTimerManager().SetTimer(
			ReturnToLobbyTravelTimerHandle,
			this,
			&APRRiftGameMode::PerformReturnToLobbyTravel,
			ReturnToLobbyDelayAfterSettlement,
			false);
		return;
	}

	PerformReturnToLobbyTravel();
}

void APRRiftGameMode::PerformReturnToLobbyTravel()
{
	if (!HasAuthority())
	{
		return;
	}

	const FString TravelURL = BuildReturnToLobbyTravelURL();
	if (TravelURL.IsEmpty())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift return travel skipped because lobby map path is empty."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift return travel skipped because world is missing: %s"), *TravelURL);
		return;
	}

	UE_LOG(LogProjectA, Log, TEXT("Rift settlement complete. Returning to lobby via ServerTravel: %s"), *TravelURL);
	LogFlowPhase(TEXT("ReturnExecuting"));
	if (!World->ServerTravel(TravelURL))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Rift return ServerTravel failed: %s"), *TravelURL);
		LogFlowPhase(TEXT("ReturnFailed"));
	}
}

void APRRiftGameMode::LogFlowPhase(const TCHAR* Phase, const APlayerState* PlayerState) const
{
	UE_LOG(
		LogProjectRiftFlow,
		Log,
		TEXT("MissionId=%s RunId=%s SettlementId=%s PlayerId=%d Phase=%s NetMode=%d"),
		*MissionId.ToString(),
		*CurrentRunId.ToString(EGuidFormats::DigitsWithHyphens),
		*CurrentSettlementId.ToString(EGuidFormats::DigitsWithHyphens),
		PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE,
		Phase ? Phase : TEXT("Unknown"),
		static_cast<int32>(GetNetMode()));
}
