#include "Core/PRRiftGameMode.h"

#include "Diagnostics/PRDiagnosticsLog.h"

#include "Characters/PRCharacter.h"
#include "Core/PRSpawnManager.h"
#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Core/PRObjectiveTypeActors.h"
#include "Core/PRAssetManager.h"
#include "Enemies/PREnemyCharacter.h"
#include "Revive/PRRescueDroneActor.h"
#include "Revive/PRReviveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTypes.h"
#include "Items/PRPickupActor.h"
#include "Items/PRQuickbarComponent.h"
#include "Items/PRRewardBudgetDataAsset.h"
#include "Items/PRRewardGenerationLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PRPlayerState.h"
#include "Player/PRPlayerController.h"
#include "ProjectA.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Roles/PRRoleComponent.h"
#include "Settings/PRProjectSettings.h"
#include "Weapons/PRWeaponComponent.h"

APRRiftGameMode::APRRiftGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = APRRiftGameState::StaticClass();
	ObjectiveGraphComponent = CreateDefaultSubobject<UPRObjectiveGraphComponent>(TEXT("ObjectiveGraphComponent"));
}

void APRRiftGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!ResolveMissionContract())
	{
		UE_LOG(LogProjectA, Error, TEXT("Rift mission will not start because its authoritative mission contract is unavailable. MissionId=%s"), *MissionId.ToString());
		return;
	}
	StartRiftMission();
}

void APRRiftGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LogFlowPhase(TEXT("EndPlay"));
	GetWorldTimerManager().ClearTimer(ReturnToLobbyTravelTimerHandle);
	StopSpawnManagers();
	ClearRescueDrone();
	SpawnManagers.Reset();
	ActiveObjective = nullptr;
	ExtractedPlayerStates.Reset();
	CountedKilledEnemies.Reset();
	PendingSettlementAcknowledgements.Reset();
	ObjectiveGraphSnapshot = FPRObjectiveGraphSnapshot();
	ObjectiveItemMissingSince.Reset();
	NextObjectiveItemRecoveryCheckTime = 0.0f;
	bRiftMissionStarted = false;
	bReturnToLobbyTravelPending = false;
	bSettlementFinalizationInProgress = false;

	Super::EndPlay(EndPlayReason);
}

void APRRiftGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString RawContractId = UGameplayStatics::ParseOption(Options, TEXT("ContractId"));
	const FString RawLegacyMissionId = UGameplayStatics::ParseOption(Options, TEXT("MissionId"));
	if (RawContractId.IsEmpty() && RawLegacyMissionId.IsEmpty())
	{
		// Direct test worlds and editor launches intentionally have no travel
		// options; retain the class default mission in that legacy entry path.
		return;
	}
	FPRMissionTravelContext TravelContext;
	FString ContextDiagnostic;
	if (!FPRMissionTravelContext::ParseOptions(Options, TravelContext, &ContextDiagnostic))
	{
		MissionId = NAME_None;
		ErrorMessage = TEXT("The requested ProjectRift mission travel context is invalid.");
		return;
	}
	if (TravelContext.ContractId.IsNone())
	{
		return;
	}
	const FName RequestedMissionName = TravelContext.ContractId;
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRMissionProgressionDataAsset* Mission = AssetManager ? AssetManager->LoadMissionSync(RequestedMissionName) : nullptr;
	if (!Mission || Mission->MissionId != RequestedMissionName || !Mission->IsContractValid())
	{
		MissionId = NAME_None;
		ErrorMessage = TEXT("The requested ProjectRift mission contract is invalid or unregistered.");
		UE_LOG(LogProjectA, Error, TEXT("Rift mission option rejected. MissionId=%s"), *RequestedMissionName.ToString());
		return;
	}
	MissionId = RequestedMissionName;
	if (TravelContext.ContractVersion > 0)
	{
		if (TravelContext.ContractVersion != Mission->Contract.ContractVersion)
		{
			MissionId = NAME_None;
			ErrorMessage = TEXT("The requested ProjectRift mission contract version is unavailable.");
			return;
		}
		CurrentMissionDefinition = Mission->BuildMissionDefinition(TravelContext.Seed);
		if (!CurrentMissionDefinition.IsValid())
		{
			MissionId = NAME_None;
			ErrorMessage = TEXT("The requested ProjectRift mission definition is invalid.");
		}
	}
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
		UpdateObjectiveItemRecovery();
	}
}

void APRRiftGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Difficulty is frozen at mission start.  A pawn can become available after
	// BeginPlay/PostLogin, so only finish provisioning the already-authorized solo
	// safety net; never recalculate the frozen participant count here.
	if (bRiftMissionStarted && !RescueDrone && GetMissionDifficultyPlayerCount() == 1)
	{
		InitializeRescueDroneForMission();
	}
	UpdateAlivePlayerCount();
	RestoreObjectiveGraphSnapshot();
}

void APRRiftGameMode::Logout(AController* Exiting)
{
	CacheObjectiveGraphSnapshot();
	if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(Exiting))
	{
		PendingSettlementAcknowledgements.Remove(TObjectKey<APRPlayerController>(ProjectRiftController));
	}
	Super::Logout(Exiting);

	UpdateAlivePlayerCount();
	TryCompleteSettlementReturn();
}

bool APRRiftGameMode::StartRiftMission()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (!ResolveMissionContract())
	{
		UE_LOG(LogProjectA, Error, TEXT("Rift mission start rejected because the mission contract is invalid. MissionId=%s"), *MissionId.ToString());
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
	if (!CurrentMissionDefinition.IsValid())
	{
		if (const UPRMissionProgressionDataAsset* Mission = ResolveMissionContract())
		{
			CurrentMissionDefinition = Mission->BuildMissionDefinition(FMath::RandRange(1, MAX_int32));
		}
	}
	if (!CurrentMissionDefinition.IsValid())
	{
		return false;
	}
	CurrentRunSeed = CurrentMissionDefinition.Seed;
	RiftGameState->SetTeamMissionDefinition(CurrentMissionDefinition, true);
	EligibleRewardProfileIds.Reset();
	if (const AGameStateBase* CurrentGameState = GameState)
	{
		for (const TObjectPtr<APlayerState>& RawPlayerState : CurrentGameState->PlayerArray)
		{
			if (const APRPlayerState* PlayerState = Cast<APRPlayerState>(RawPlayerState.Get()))
			{
				if (PlayerState->IsMultiplayerProfileBound())
				{
					EligibleRewardProfileIds.AddUnique(PlayerState->GetBoundProfileId());
				}
			}
		}
	}
	FinalizedRunId.Invalidate();
	bSettlementFinalizationInProgress = false;
	PendingSettlementAcknowledgements.Reset();
	FailedSettlementAcknowledgementCount = 0;
	CountedKilledEnemies.Reset();
	ObjectiveGraphSnapshot = FPRObjectiveGraphSnapshot();
	ObjectiveItemMissingSince.Reset();
	NextObjectiveItemRecoveryCheckTime = 0.0f;
	bRiftMissionStarted = true;
	ActiveObjective = nullptr;
	ResetExtractionState();
	StopSpawnManagers();
	ClearRescueDrone();
	RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::NotStarted);
	RiftGameState->SetObjectiveProgress(0.0f);
	RiftGameState->SetObjectiveSummaries({});
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while starting a Rift mission; using the code default stability."));
	}
	const float InitialStability = ProjectSettings
		? FMath::Clamp(ProjectSettings->InitialRiftStability, 0.0f, 100.0f)
		: 100.0f;
	RiftGameState->SetRiftStability(InitialStability);
	RiftGameState->SetExtractionAvailable(false);
	RiftGameState->SetMissionTime(0.0f);
	RiftGameState->SetKilledEnemyCount(0);
	RiftGameState->SetDifficultyPlayerCount(CountMissionParticipants());
	InitializeRescueDroneForMission();
	UpdateAlivePlayerCount();
	if (!InitializeObjectiveGraph())
	{
		return false;
	}

	UE_LOG(LogProjectA, Log, TEXT("Rift mission started. Stability=%.1f AlivePlayers=%d"),
		RiftGameState->GetRiftStability(),
		RiftGameState->GetAlivePlayerCount());
	LogFlowPhase(TEXT("MissionStarted"));

	return true;
}

int32 APRRiftGameMode::AllocateRewardSeed(
	const EPRRewardSourceType SourceType,
	const FName SourceId,
	const FGuid RecipientProfileId,
	const int32 Ordinal) const
{
	FPRRewardSourceContext Source;
	Source.SourceType = SourceType;
	Source.SourceId = SourceId;
	Source.RecipientProfileId = RecipientProfileId;
	Source.Ordinal = Ordinal;
	return UPRRewardGenerationLibrary::DeriveSeed(CurrentRunSeed, Source);
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

bool APRRiftGameMode::HasObjectiveGraph() const
{
	return ObjectiveGraphComponent && !ObjectiveGraphComponent->GetDefinition().Nodes.IsEmpty();
}

bool APRRiftGameMode::CanActivateObjectiveNode(const FName NodeId) const
{
	return HasObjectiveGraph() && ObjectiveGraphComponent->GetNodeState(NodeId) == EPRObjectiveNodeState::Available;
}

bool APRRiftGameMode::ActivateObjectiveNode(APRRiftObjectiveActor* ObjectiveActor, AController* ActivatingController)
{
	if (!HasAuthority() || !ObjectiveActor || !HasObjectiveGraph())
	{
		return false;
	}
	const bool bActivated = ObjectiveGraphComponent->ActivateNode(ObjectiveActor->GetObjectiveNodeId());
	if (bActivated)
	{
		RefreshObjectiveGraphReplication();
	}
	return bActivated;
}

bool APRRiftGameMode::ReportObjectiveNodeProgress(const FName NodeId, const float NormalizedProgress)
{
	if (!HasAuthority() || !HasObjectiveGraph() || !ObjectiveGraphComponent->SetNodeProgressNormalized(NodeId, NormalizedProgress))
	{
		return false;
	}
	RefreshObjectiveGraphReplication();
	SynchronizeObjectiveGraphActors();
	if (ObjectiveGraphComponent->IsGraphCompleted())
	{
		CompleteCurrentObjective();
	}
	CacheObjectiveGraphSnapshot();
	return true;
}

bool APRRiftGameMode::ReportObjectiveNodeCount(const FName NodeId, const int32 CurrentCount)
{
	if (!HasAuthority() || !HasObjectiveGraph() || !ObjectiveGraphComponent->SetNodeCurrentCount(NodeId, CurrentCount))
	{
		return false;
	}
	RefreshObjectiveGraphReplication();
	SynchronizeObjectiveGraphActors();
	if (ObjectiveGraphComponent->IsGraphCompleted())
	{
		CompleteCurrentObjective();
	}
	CacheObjectiveGraphSnapshot();
	return true;
}

bool APRRiftGameMode::RegisterObjectivePickup(const FName NodeId, const FGuid ItemInstanceGuid)
{
	if (!HasAuthority() || !CanRegisterObjectivePickup(NodeId, ItemInstanceGuid))
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<APRRiftObjectiveActor> It(World); It; ++It)
	{
		APRRiftObjectiveActor* Objective = *It;
		if (!Objective || Objective->GetObjectiveNodeId() != NodeId)
		{
			continue;
		}
		if (APRCollectObjectiveActor* CollectObjective = Cast<APRCollectObjectiveActor>(Objective))
		{
			const bool bRegistered = CollectObjective->RegisterCollectedItem(ItemInstanceGuid);
			if (bRegistered) { CacheObjectiveGraphSnapshot(); }
			return bRegistered;
		}
		if (APRCarryObjectiveActor* CarryObjective = Cast<APRCarryObjectiveActor>(Objective))
		{
			const bool bRegistered = CarryObjective->SetTrackedCoreGuid(ItemInstanceGuid);
			if (bRegistered) { CacheObjectiveGraphSnapshot(); }
			return bRegistered;
		}
	}
	return false;
}

bool APRRiftGameMode::CanRegisterObjectivePickup(const FName NodeId, const FGuid ItemInstanceGuid) const
{
	if (!HasObjectiveGraph() || NodeId.IsNone() || !ItemInstanceGuid.IsValid()
		|| ObjectiveGraphComponent->GetNodeState(NodeId) != EPRObjectiveNodeState::Active)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<APRRiftObjectiveActor> It(World); It; ++It)
	{
		const APRRiftObjectiveActor* Objective = *It;
		if (!Objective || Objective->GetObjectiveNodeId() != NodeId)
		{
			continue;
		}
		if (const APRCollectObjectiveActor* CollectObjective = Cast<APRCollectObjectiveActor>(Objective))
		{
			return !CollectObjective->HasCollectedItem(ItemInstanceGuid);
		}
		if (const APRCarryObjectiveActor* CarryObjective = Cast<APRCarryObjectiveActor>(Objective))
		{
			return !CarryObjective->GetTrackedCoreGuid().IsValid();
		}
	}
	return false;
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
	ClearRescueDrone();
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

float APRRiftGameMode::GetReturnToLobbyDelayAfterSettlement() const
{
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while reading the settlement return delay; using the code default."));
	}
	return ProjectSettings
		? FMath::Max(0.0f, ProjectSettings->ReturnToLobbyDelayAfterSettlement)
		: 4.0f;
}

float APRRiftGameMode::GetSettlementAcknowledgementTimeout() const
{
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	return ProjectSettings ? FMath::Max(0.0f, ProjectSettings->SettlementAcknowledgementTimeout) : 8.0f;
}

FPRRiftSettlementData APRRiftGameMode::GenerateSettlementData(const EPRRiftMissionResult Result) const
{
	FPRRiftSettlementData Settlement;
	Settlement.MissionId = MissionId;
	Settlement.ContractVersion = CurrentMissionDefinition.ContractVersion;
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
		ClearRescueDrone();
		FPRRiftSettlementData Settlement = GenerateSettlementData(Result);
		ApplyExtractedResourceRules(Result, Settlement);
		FinalizePersonalSettlements(Result);

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

void APRRiftGameMode::FinalizePersonalSettlements(const EPRRiftMissionResult Result)
{
	PendingSettlementAcknowledgements.Reset();
	FailedSettlementAcknowledgementCount = 0;
	const UPRMissionProgressionDataAsset* Mission = ResolveMissionContract();
	const AGameStateBase* CurrentGameState = GameState;
	if (!Mission || !CurrentGameState)
	{
		UE_LOG(LogProjectA, Error, TEXT("Personal settlement skipped because the mission contract or GameState is unavailable."));
		return;
	}

	for (const TObjectPtr<APlayerState>& RawPlayerState : CurrentGameState->PlayerArray)
	{
		APRPlayerState* PlayerState = Cast<APRPlayerState>(RawPlayerState.Get());
		APRPlayerController* PlayerController = PlayerState ? Cast<APRPlayerController>(PlayerState->GetOwner()) : nullptr;
		if (!PlayerState || !PlayerController || PlayerState->IsOnlyASpectator() || !PlayerState->IsMultiplayerProfileBound())
		{
			continue;
		}

		const FPRPlayerSettlementReceipt Receipt = BuildPersonalSettlementReceipt(PlayerState, Result, Mission);
		FString Diagnostic;
		if (!Receipt.IsValid(&Diagnostic))
		{
			UE_LOG(LogProjectA, Error, TEXT("Personal settlement receipt rejected before dispatch. Player=%s Diagnostic=%s"), *GetNameSafe(PlayerState), *Diagnostic);
			continue;
		}

		UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
		UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent();
		TArray<FPRItemInstance> PreviousBackpack;
		FString EquipmentDiagnostic;
		const TArray<FPRProfileEquipmentEntry> PreviousEquipment = Weapon
			? Weapon->GetEquipmentEntries()
			: TArray<FPRProfileEquipmentEntry>();
		const bool bPreviousWeaponStateCaptured = Weapon
			&& Weapon->BuildPersistentBackpack(PreviousBackpack, EquipmentDiagnostic);
		if (!Inventory || !Weapon
			|| !bPreviousWeaponStateCaptured
			|| !Inventory->ReplaceInventoryItems(Receipt.SettledBackpackItems)
			|| !Weapon->ReplaceEquipmentEntries(Receipt.SettledEquipment, EquipmentDiagnostic))
		{
			if (Inventory && Weapon && bPreviousWeaponStateCaptured)
			{
				Inventory->ReplaceInventoryItems(PreviousBackpack);
				FString RestoreDiagnostic;
				Weapon->ReplaceEquipmentEntries(PreviousEquipment, RestoreDiagnostic);
			}
			UE_LOG(LogProjectA, Error, TEXT("Settled weapon inventory could not be applied to PlayerState. Player=%s Diagnostic=%s"), *GetNameSafe(PlayerState), *EquipmentDiagnostic);
			continue;
		}
		TArray<FPRShipResourceStack> SettledResources;
		SettledResources.Reserve(Receipt.SettledResourceWallet.Num());
		for (const FPRProfileResourceBalance& Resource : Receipt.SettledResourceWallet)
		{
			FPRShipResourceStack& SettledResource = SettledResources.AddDefaulted_GetRef();
			SettledResource.ResourceId = Resource.ResourceId;
			SettledResource.Count = Resource.Count;
		}
		if (!PlayerState->ReplaceShipResources(SettledResources))
		{
			UE_LOG(LogProjectA, Error, TEXT("Settled resource wallet could not be applied to PlayerState. Player=%s"), *GetNameSafe(PlayerState));
			continue;
		}
		PlayerState->SetSelectedRoleModule(Receipt.SettledRoleId);
		if (Receipt.bGrantStoryProgress)
		{
			PlayerState->ApplyMissionCompletion(Mission);
		}

		PendingSettlementAcknowledgements.Add(TObjectKey<APRPlayerController>(PlayerController));
		PlayerController->ClientReceivePersonalSettlement(Receipt);
	}
}

FPRPlayerSettlementReceipt APRRiftGameMode::BuildPersonalSettlementReceipt(
	APRPlayerState* PlayerState,
	const EPRRiftMissionResult Result,
	const UPRMissionProgressionDataAsset* Mission) const
{
	FPRPlayerSettlementReceipt Receipt;
	if (!PlayerState || !Mission)
	{
		return Receipt;
	}
	Receipt.ProfileId = PlayerState->GetBoundProfileId();
	Receipt.MissionId = MissionId;
	Receipt.ContractVersion = CurrentMissionDefinition.ContractVersion;
	Receipt.RunId = CurrentRunId;
	Receipt.RunSeed = CurrentRunSeed;
	Receipt.SettlementId = CurrentSettlementId;
	Receipt.Result = Result;
	Receipt.bExtracted = ExtractedPlayerStates.Contains(TObjectKey<APlayerState>(PlayerState));
	Receipt.bGrantStoryProgress = Result == EPRRiftMissionResult::Success
		&& Receipt.bExtracted
		&& Mission->IsEligible(PlayerState->GetBoundStoryProgress());
	const UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent();
	TArray<FPRItemInstance> RuntimeItems;
	FString WeaponDiagnostic;
	if (!Weapon || !Weapon->BuildPersistentBackpack(RuntimeItems, WeaponDiagnostic))
	{
		return FPRPlayerSettlementReceipt();
	}
	Receipt.SettledBackpackItems = Result == EPRRiftMissionResult::Success && Receipt.bExtracted
		? RuntimeItems
		: FPRMultiplayerSettlementPolicy::BuildNonExtractedInventory(PlayerState->GetMissionStartBackpackItems(), RuntimeItems);
	if (const UPRQuickbarComponent* Quickbar = PlayerState->GetQuickbarComponent())
	{
		Receipt.SettledQuickSlots = Quickbar->GetQuickSlots();
	}
	if (const UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent())
	{
		TSet<FGuid> AutoProcessedInstances;
		Receipt.SettledBackpackItems.RemoveAll([Inventory, &AutoProcessedInstances](const FPRItemInstance& Item)
		{
			const UPRItemDataAsset* Data = Inventory->FindItemData(Item.ItemId);
			if (Data && Data->bAutoProcessAtMissionEnd)
			{
				AutoProcessedInstances.Add(Item.InstanceGuid);
				return true;
			}
			return false;
		});
		if (AutoProcessedInstances.Num() > 0)
		{
			Receipt.SettledQuickSlots.RemoveAll([&AutoProcessedInstances](const FPRQuickSlotReference& Slot)
			{
				return AutoProcessedInstances.Contains(Slot.InstanceGuid);
			});
		}
	}
	Receipt.SettledEquipment = Weapon->GetEquipmentEntries();
	for (const FPRShipResourceStack& Resource : PlayerState->GetShipResources())
	{
		if (Resource.IsValid())
		{
			Receipt.SettledResourceWallet.Emplace(Resource.ResourceId, Resource.Count);
		}
	}
	if (Result == EPRRiftMissionResult::Success && !Receipt.bExtracted)
	{
		Receipt.SettledResourceWallet = FPRMultiplayerSettlementPolicy::BuildNonExtractedResourceWallet(
			PlayerState->GetMissionStartResourceWallet(),
			Receipt.SettledResourceWallet);
	}
	if (const UPRRoleComponent* RoleComponent = PlayerState->GetRoleComponent())
	{
		FPRRoleLoadout RuntimeLoadout;
		RoleComponent->CaptureProfileRoleState(
			Receipt.SettledRoleId,
			Receipt.SettledUnlockedRoleIds,
			RuntimeLoadout,
			Receipt.SettledUnlockedRoleModuleIds);
		Receipt.SettledEquippedRoleModules = RuntimeLoadout.Entries;
	}
	else
	{
		Receipt.SettledRoleId = PlayerState->GetSelectedRoleModule();
	}
	if (Result == EPRRiftMissionResult::Success && Receipt.bExtracted && EligibleRewardProfileIds.Contains(Receipt.ProfileId))
	{
		if (UPRRewardBudgetDataAsset* RewardBudget = Mission->RewardBudget.LoadSynchronous())
		{
			FPRRewardSourceContext Source;
			Source.SourceType = EPRRewardSourceType::MissionSettlement;
			Source.SourceId = MissionId;
			Source.RecipientProfileId = Receipt.ProfileId;
			Source.Ordinal = 0;
			Source.Seed = AllocateRewardSeed(Source.SourceType, Source.SourceId, Source.RecipientProfileId, Source.Ordinal);
			const FPRPersonalRewardGenerationResult Generated = UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(
				RewardBudget,
				Source,
				PlayerState->GetLootProtectionState(RewardBudget->GetFName()),
				GetMissionDifficultyPlayerCount());
			if (Generated.bSuccess)
			{
				Receipt.GrantedWarehouseItems = Generated.GrantedWarehouseItems;
				Receipt.UpdatedLootProtectionState = Generated.UpdatedProtectionState;
				Receipt.RewardAuditEntries = Generated.AuditEntries;
				if (Generated.CommonChipCount > 0)
				{
					const int32 ExistingIndex = Receipt.SettledResourceWallet.IndexOfByPredicate([](const FPRProfileResourceBalance& Balance)
					{
						return Balance.ResourceId == TEXT("CommonChip");
					});
					if (ExistingIndex == INDEX_NONE)
					{
						Receipt.SettledResourceWallet.Emplace(TEXT("CommonChip"), Generated.CommonChipCount);
					}
					else
					{
						Receipt.SettledResourceWallet[ExistingIndex].Count += Generated.CommonChipCount;
					}
				}
			}
		}
	}
	return Receipt;
}

UPRMissionProgressionDataAsset* APRRiftGameMode::ResolveMissionContract() const
{
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRMissionProgressionDataAsset* Mission = AssetManager ? AssetManager->LoadMissionSync(MissionId) : nullptr;
	return Mission && Mission->MissionId == MissionId && Mission->IsContractValid() ? Mission : nullptr;
}

void APRRiftGameMode::HandlePersonalSettlementAcknowledgement(
	APRPlayerController* PlayerController,
	const FGuid SettlementId,
	const bool bSaveSucceeded)
{
	if (!HasAuthority() || !PlayerController || SettlementId != CurrentSettlementId)
	{
		return;
	}
	const int32 RemovedCount = PendingSettlementAcknowledgements.Remove(TObjectKey<APRPlayerController>(PlayerController));
	if (RemovedCount > 0 && !bSaveSucceeded)
	{
		++FailedSettlementAcknowledgementCount;
	}
	if (RemovedCount > 0)
	{
		LogFlowPhase(bSaveSucceeded ? TEXT("PersonalSettlementSaved") : TEXT("PersonalSettlementSaveFailed"), PlayerController->PlayerState);
		TryCompleteSettlementReturn();
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
		const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
		if (!ProjectSettings)
		{
			UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while calculating failed resource retention; using the code default."));
		}
		const float RetentionRate = ProjectSettings
			? FMath::Clamp(ProjectSettings->FailedResourceRetentionRate, 0.0f, 1.0f)
			: 0.5f;
		return FMath::FloorToInt(SafeCount * RetentionRate);
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

void APRRiftGameMode::HandlePlayerDowned(APRCharacter* DownedCharacter)
{
	if (!HasAuthority() || !DownedCharacter)
	{
		return;
	}
	if (GetMissionDifficultyPlayerCount() == 1 && RescueDrone && RescueDrone->GetAssignedPlayer() == DownedCharacter)
	{
		RescueDrone->RequestRescue(DownedCharacter);
	}
	UpdateAlivePlayerCount();
}

void APRRiftGameMode::HandlePlayerRevived(APRCharacter* RevivedCharacter)
{
	if (HasAuthority())
	{
		UpdateAlivePlayerCount();
	}
}

void APRRiftGameMode::HandlePlayerBleedOut(APRCharacter* DeadCharacter)
{
	if (HasAuthority())
	{
		UpdateAlivePlayerCount();
	}
}

int32 APRRiftGameMode::GetMissionDifficultyPlayerCount() const
{
	const APRRiftGameState* RiftGameState = GetRiftGameState();
	return RiftGameState ? FMath::Clamp(RiftGameState->GetDifficultyPlayerCount(), 1, 4) : 1;
}

void APRRiftGameMode::HandleObjectiveActivated(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority() || !ObjectiveActor)
	{
		return;
	}

	ActiveObjective = ObjectiveActor;
	if (!ObjectiveActor->UsesObjectiveGraph() || (ObjectiveGraphComponent && ObjectiveGraphComponent->FindNodeDefinition(ObjectiveActor->GetObjectiveNodeId())
		&& ObjectiveGraphComponent->FindNodeDefinition(ObjectiveActor->GetObjectiveNodeId())->bDrivesEnemySpawning))
	{
		StartSpawnManagersForObjective(ObjectiveActor);
	}
	LogFlowPhase(TEXT("ObjectiveActivated"));

	if (APRRiftGameState* RiftGameState = GetRiftGameState())
	{
		RiftGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::Active);
		RiftGameState->SetObjectiveProgress(HasObjectiveGraph() ? 0.0f : ObjectiveActor->GetObjectiveProgress());
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
	if (ObjectiveActor->UsesObjectiveGraph())
	{
		ReportObjectiveNodeProgress(ObjectiveActor->GetObjectiveNodeId(), ObjectiveProgress);
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
	if (ObjectiveActor->UsesObjectiveGraph())
	{
		ReportObjectiveNodeProgress(ObjectiveActor->GetObjectiveNodeId(), 1.0f);
		LogFlowPhase(TEXT("ObjectiveNodeCompleted"));
		return;
	}
	StopSpawnManagers();
	LogFlowPhase(TEXT("ObjectiveCompleted"));
	CompleteCurrentObjective();
}

bool APRRiftGameMode::InitializeObjectiveGraph()
{
	const UPRMissionProgressionDataAsset* Mission = ResolveMissionContract();
	if (!Mission || !Mission->HasObjectiveGraph())
	{
		return true;
	}
	if (!ObjectiveGraphComponent)
	{
		UE_LOG(LogProjectA, Error, TEXT("Rift objective graph is missing its runtime component."));
		return false;
	}
	FString Diagnostic;
	if (!ObjectiveGraphComponent->InitializeGraph(Mission->ObjectiveGraph, &Diagnostic))
	{
		UE_LOG(LogProjectA, Error, TEXT("Rift objective graph is invalid. MissionId=%s Diagnostic=%s"), *MissionId.ToString(), *Diagnostic);
		return false;
	}
	RefreshObjectiveGraphReplication();
	SynchronizeObjectiveGraphActors();
	return true;
}

void APRRiftGameMode::RefreshObjectiveGraphReplication()
{
	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || !HasObjectiveGraph())
	{
		return;
	}
	const TArray<FPRObjectiveSummary> Summaries = ObjectiveGraphComponent->GetVisibleSummaries();
	RiftGameState->SetObjectiveSummaries(Summaries);
	float RequiredProgress = 0.0f;
	int32 RequiredCount = 0;
	for (const FPRObjectiveSummary& Summary : Summaries)
	{
		if (!Summary.bOptional)
		{
			RequiredProgress += Summary.Progress;
			++RequiredCount;
		}
	}
	RiftGameState->SetObjectiveProgress(RequiredCount > 0 ? RequiredProgress / RequiredCount : 0.0f);
	RiftGameState->SetCurrentObjectiveState(ObjectiveGraphComponent->IsGraphCompleted()
		? EPRRiftObjectiveState::Completed
		: EPRRiftObjectiveState::Active);
}

void APRRiftGameMode::SynchronizeObjectiveGraphActors()
{
	if (!HasAuthority() || !HasObjectiveGraph())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<APRRiftObjectiveActor> It(World); It; ++It)
	{
		APRRiftObjectiveActor* Objective = *It;
		if (Objective && Objective->UsesObjectiveGraph()
			&& ObjectiveGraphComponent->GetNodeState(Objective->GetObjectiveNodeId()) == EPRObjectiveNodeState::Active)
		{
			Objective->ActivateFromObjectiveGraph();
		}
	}
}

void APRRiftGameMode::CacheObjectiveGraphSnapshot()
{
	if (HasObjectiveGraph())
	{
		ObjectiveGraphSnapshot = ObjectiveGraphComponent->BuildSnapshot();
	}
}

void APRRiftGameMode::RestoreObjectiveGraphSnapshot()
{
	if (!HasAuthority() || !HasObjectiveGraph() || !ObjectiveGraphSnapshot.IsValid())
	{
		return;
	}
	FString Diagnostic;
	if (!ObjectiveGraphComponent->RestoreGraph(ObjectiveGraphComponent->GetDefinition(), ObjectiveGraphSnapshot, &Diagnostic))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Objective graph reconnect restore rejected: %s"), *Diagnostic);
		return;
	}
	RefreshObjectiveGraphReplication();
	SynchronizeObjectiveGraphActors();
}

bool APRRiftGameMode::IsObjectiveItemAvailable(const FGuid ItemInstanceGuid) const
{
	if (!ItemInstanceGuid.IsValid())
	{
		return false;
	}
	if (const AGameStateBase* CurrentGameState = GameState)
	{
		for (const TObjectPtr<APlayerState>& RawPlayerState : CurrentGameState->PlayerArray)
		{
			const APRPlayerState* PlayerState = Cast<APRPlayerState>(RawPlayerState.Get());
			const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
			if (Inventory && Inventory->GetItems().ContainsByPredicate([ItemInstanceGuid](const FPRItemInstance& Item)
			{
				return Item.InstanceGuid == ItemInstanceGuid && Item.Count > 0;
			}))
			{
				return true;
			}
		}
	}
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<APRPickupActor> It(World); It; ++It)
		{
			const APRPickupActor* Pickup = *It;
			if (Pickup && Pickup->CanBePickedUp() && Pickup->GetItemInstance().InstanceGuid == ItemInstanceGuid)
			{
				return true;
			}
		}
	}
	return false;
}

void APRRiftGameMode::UpdateObjectiveItemRecovery()
{
	if (!HasAuthority() || !HasObjectiveGraph())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() < NextObjectiveItemRecoveryCheckTime)
	{
		return;
	}
	NextObjectiveItemRecoveryCheckTime = World->GetTimeSeconds() + 1.0f;
	for (TActorIterator<APRCarryObjectiveActor> It(World); It; ++It)
	{
		APRCarryObjectiveActor* CarryObjective = *It;
		const FGuid CoreGuid = CarryObjective ? CarryObjective->GetTrackedCoreGuid() : FGuid();
		if (!CarryObjective || !CarryObjective->IsObjectiveActive() || !CoreGuid.IsValid())
		{
			continue;
		}
		if (IsObjectiveItemAvailable(CoreGuid))
		{
			ObjectiveItemMissingSince.Remove(CoreGuid);
			continue;
		}
		const float* MissingSince = ObjectiveItemMissingSince.Find(CoreGuid);
		if (!MissingSince)
		{
			ObjectiveItemMissingSince.Add(CoreGuid, World->GetTimeSeconds());
			continue;
		}
		if (World->GetTimeSeconds() - *MissingSince < 10.0f)
		{
			continue;
		}
		const FPRObjectiveNodeDefinition* NodeDefinition = ObjectiveGraphComponent->FindNodeDefinition(CarryObjective->GetObjectiveNodeId());
		if (!NodeDefinition || NodeDefinition->TargetId.IsNone())
		{
			continue;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		APRPickupActor* RecoveredPickup = World->SpawnActor<APRPickupActor>(APRPickupActor::StaticClass(), CarryObjective->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), FRotator::ZeroRotator, SpawnParameters);
		if (!RecoveredPickup)
		{
			continue;
		}
		FPRItemInstance RecoveredItem;
		RecoveredItem.ItemId = NodeDefinition->TargetId;
		RecoveredItem.Count = 1;
		RecoveredItem.InstanceGuid = CoreGuid;
		RecoveredPickup->SetItemInstance(RecoveredItem);
		RecoveredPickup->SetObjectiveNodeId(CarryObjective->GetObjectiveNodeId());
		ObjectiveItemMissingSince.Remove(CoreGuid);
		UE_LOG(LogProjectA, Log, TEXT("Recovered missing carry objective item. Node=%s Guid=%s"), *CarryObjective->GetObjectiveNodeId().ToString(), *CoreGuid.ToString(EGuidFormats::DigitsWithHyphens));
	}
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
	if (!Enemy->GetHuntTargetId().IsNone() && HasObjectiveGraph())
	{
		for (TActorIterator<APRRiftObjectiveActor> It(GetWorld()); It; ++It)
		{
			if (APRHuntObjectiveActor* HuntObjective = Cast<APRHuntObjectiveActor>(*It))
			{
				HuntObjective->RegisterHuntTargetEliminated(Enemy->GetHuntTargetId());
			}
		}
	}
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

int32 APRRiftGameMode::CountMissionParticipants() const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		return 1;
	}
	int32 PlayerCount = 0;
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		if (PlayerState && !PlayerState->IsOnlyASpectator())
		{
			++PlayerCount;
		}
	}
	return FMath::Clamp(PlayerCount, 1, 4);
}

void APRRiftGameMode::InitializeRescueDroneForMission()
{
	ClearRescueDrone();
	if (!HasAuthority() || GetMissionDifficultyPlayerCount() != 1)
	{
		return;
	}
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState || !GetWorld())
	{
		return;
	}
	for (const TObjectPtr<APlayerState>& PlayerState : CurrentGameState->PlayerArray)
	{
		if (!PlayerState || PlayerState->IsOnlyASpectator())
		{
			continue;
		}
		APRCharacter* Character = Cast<APRCharacter>(ResolvePawnForPlayerState(PlayerState.Get()));
		if (!Character)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = Character;
		Params.Instigator = Character;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RescueDrone = GetWorld()->SpawnActor<APRRescueDroneActor>(APRRescueDroneActor::StaticClass(), Character->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (RescueDrone && !RescueDrone->InitializeForPlayer(Character))
		{
			RescueDrone->Destroy();
			RescueDrone = nullptr;
		}
		return;
	}
}

void APRRiftGameMode::ClearRescueDrone()
{
	if (RescueDrone)
	{
		RescueDrone->ShutdownDrone();
		RescueDrone = nullptr;
	}
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

	if (bFoundEligiblePlayer && GetMissionDifficultyPlayerCount() == 1 && RescueDrone && RescueDrone->CanProvideRecovery())
	{
		return false;
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
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while draining Rift stability; using the code default."));
	}
	const float DrainPerSecond = ProjectSettings
		? FMath::Max(0.0f, ProjectSettings->RiftStabilityDrainPerSecond)
		: 1.0f;
	if (DrainPerSecond <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || RiftGameState->GetCurrentObjectiveState() != EPRRiftObjectiveState::Active)
	{
		return;
	}

	RiftGameState->SetRiftStability(RiftGameState->GetRiftStability() - DrainPerSecond * DeltaSeconds);
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

	const float Now = World->GetTimeSeconds();
	const float ReturnDelay = GetReturnToLobbyDelayAfterSettlement();
	SettlementReturnEarliestTime = Now + ReturnDelay;
	SettlementAcknowledgementDeadline = Now + FMath::Max(ReturnDelay, GetSettlementAcknowledgementTimeout());
	UE_LOG(LogProjectA, Log, TEXT("Rift settlement return barrier armed. DisplayDelay=%.1f AckTimeout=%.1f Pending=%d Travel=%s"),
		ReturnDelay,
		GetSettlementAcknowledgementTimeout(),
		PendingSettlementAcknowledgements.Num(),
		*TravelURL);
	TryCompleteSettlementReturn();
}

void APRRiftGameMode::TryCompleteSettlementReturn()
{
	if (!HasAuthority() || !bReturnToLobbyTravelPending || !bReturnToLobbyServerTravelEnabled)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now < SettlementReturnEarliestTime)
	{
		World->GetTimerManager().SetTimer(ReturnToLobbyTravelTimerHandle, this, &APRRiftGameMode::TryCompleteSettlementReturn, SettlementReturnEarliestTime - Now, false);
		return;
	}
	if (PendingSettlementAcknowledgements.IsEmpty() || Now >= SettlementAcknowledgementDeadline)
	{
		if (!PendingSettlementAcknowledgements.IsEmpty())
		{
			UE_LOG(LogProjectA, Warning, TEXT("Personal settlement acknowledgement timeout. Pending=%d Failed=%d"), PendingSettlementAcknowledgements.Num(), FailedSettlementAcknowledgementCount);
		}
		PerformReturnToLobbyTravel();
		return;
	}
	World->GetTimerManager().SetTimer(ReturnToLobbyTravelTimerHandle, this, &APRRiftGameMode::TryCompleteSettlementReturn, SettlementAcknowledgementDeadline - Now, false);
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
	FPRDiagnosticContext Context;
	Context.PlayerId = PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
	Context.RunId = CurrentRunId;
	Context.SettlementId = CurrentSettlementId;
	PRRecordDiagnosticEvent(
		this,
		Phase && FCString::Strstr(Phase, TEXT("Failed")) ? EPRDiagnosticSeverity::Error : EPRDiagnosticSeverity::Info,
		TEXT("Flow"),
		FName(*FString::Printf(TEXT("Rift.%s"), Phase ? Phase : TEXT("Unknown"))),
		FString::Printf(TEXT("Mission %s entered flow phase %s."), *MissionId.ToString(), Phase ? Phase : TEXT("Unknown")),
		Context);
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
