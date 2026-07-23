#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameModeBase.h"
#include "Core/PRRiftGameState.h"
#include "Items/PRItemTypes.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "UObject/ObjectKey.h"
#include "PRRiftGameMode.generated.h"

class APRRiftObjectiveActor;
class APRSpawnManager;
class APRPlayerState;
class APREnemyCharacter;
class APlayerState;
class APawn;
class UPRInventoryComponent;
class APRPlayerController;
class UPRMissionProgressionDataAsset;
class APRRescueDroneActor;
class APRCharacter;
class UPRObjectiveGraphComponent;
class UPRRiftRuleComponent;
class UPREncounterDirectorComponent;
class APRBossObjectiveActor;

/**
 * Server-authoritative rule set for rift missions.
 */
UCLASS()
class PROJECTA_API APRRiftGameMode : public APRGameModeBase
{
	GENERATED_BODY()

public:
	APRRiftGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	bool StartRiftMission();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void CompleteCurrentObjective();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void OpenExtraction();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	bool RegisterPlayerExtracted(AController* ExtractingController);

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	bool IsPlayerExtracted(AController* Controller) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void ResetExtractionState();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void CheckExtractionCompletion();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	bool RequestRiftFailure();

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	FString GetReturnToLobbyMapPath() const { return ReturnToLobbyMapPath; }

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	FString BuildReturnToLobbyTravelURL() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	bool IsReturnToLobbyTravelPending() const { return bReturnToLobbyTravelPending; }

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	float GetReturnToLobbyDelayAfterSettlement() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Extraction")
	void SetReturnToLobbyServerTravelEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	int32 GetExtractedPlayerCount() const { return ExtractedPlayerStates.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void UpdateAlivePlayerCount();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Revive")
	void HandlePlayerDowned(APRCharacter* DownedCharacter);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Revive")
	void HandlePlayerRevived(APRCharacter* RevivedCharacter);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Revive")
	void HandlePlayerBleedOut(APRCharacter* DeadCharacter);

	UFUNCTION(BlueprintPure, Category = "Rift|Scaling")
	int32 GetMissionDifficultyPlayerCount() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Revive")
	APRRescueDroneActor* GetRescueDrone() const { return RescueDrone; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveActivated(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveProgressChanged(APRRiftObjectiveActor* ObjectiveActor, float ObjectiveProgress);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveCompleted(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	UPRObjectiveGraphComponent* GetObjectiveGraphComponent() const { return ObjectiveGraphComponent; }

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	UPRRiftRuleComponent* GetRiftRuleComponent() const { return RiftRuleComponent; }
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") UPREncounterDirectorComponent* GetEncounterDirectorComponent() const { return EncounterDirectorComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Rules")
	bool ReportRiftAlarm(FName AlarmId, int32 Severity);

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	bool HasObjectiveGraph() const;

	bool CanActivateObjectiveNode(FName NodeId) const;
	bool ActivateObjectiveNode(APRRiftObjectiveActor* ObjectiveActor, AController* ActivatingController);
	bool ReportObjectiveNodeProgress(FName NodeId, float NormalizedProgress);
	bool ReportObjectiveNodeCount(FName NodeId, int32 CurrentCount);
	/** Validates a graph-bound pickup before its inventory transaction mutates state. */
	bool CanRegisterObjectivePickup(FName NodeId, FGuid ItemInstanceGuid) const;
	bool RegisterObjectivePickup(FName NodeId, FGuid ItemInstanceGuid);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	bool StartSpawnManagersForObjective(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	void StopSpawnManagers();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Combat")
	bool RegisterEnemyKilled(APREnemyCharacter* Enemy, AController* Killer);

	/** Boss objectives retain Hunt semantics while recording one server-owned reward source per run. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Boss")
	bool RegisterBossDefeated(APRBossObjectiveActor* ObjectiveActor, FName RewardId);

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	bool HasRiftMissionStarted() const { return bRiftMissionStarted; }

	UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
	FGuid GetCurrentRunId() const { return CurrentRunId; }

	UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
	FGuid GetCurrentSettlementId() const { return CurrentSettlementId; }

	UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
	int32 GetCurrentRunSeed() const { return CurrentRunSeed; }

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	FPRMissionDefinition GetMissionDefinition() const { return CurrentMissionDefinition; }

	UFUNCTION(BlueprintPure, Category = "Rift|Rewards")
	int32 AllocateRewardSeed(EPRRewardSourceType SourceType, FName SourceId, FGuid RecipientProfileId, int32 Ordinal) const;

	UFUNCTION(BlueprintPure, Category = "Rift|Diagnostics")
	FName GetMissionId() const { return MissionId; }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	FPRRiftSettlementData GenerateSettlementData(EPRRiftMissionResult Result) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Settlement")
	void FinalizeRiftSettlement(EPRRiftMissionResult Result);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Settlement")
	void HandlePersonalSettlementAcknowledgement(APRPlayerController* PlayerController, FGuid SettlementId, bool bSaveSucceeded);

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	float GetSettlementAcknowledgementTimeout() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	int32 GetPendingSettlementAcknowledgementCount() const { return PendingSettlementAcknowledgements.Num(); }

	UFUNCTION(BlueprintPure, Category = "Rift|Resources")
	int32 CalculateRetainedResourceCount(int32 Count, EPRRiftMissionResult Result) const;

	UFUNCTION(BlueprintPure, Category = "Rift|Resources")
	bool IsExtractableResourceItem(const FPRItemInstance& Item, UPRInventoryComponent* InventoryComponent) const;

protected:
	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	APRRiftGameState* GetRiftGameState() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	int32 CountAlivePlayers() const;
	int32 CountMissionParticipants() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Extraction")
	FString ReturnToLobbyMapPath = TEXT("/Game/ProjectRift/Maps/L_ShipLobby");

private:
	bool CheckFailureConditions();
	bool AreAllActivePlayersDefeated() const;
	void InitializeRescueDroneForMission();
	void ClearRescueDrone();
	bool IsPlayerStateAliveForRift(const APlayerState* PlayerState) const;
	APawn* ResolvePawnForPlayerState(const APlayerState* PlayerState) const;
	void DrainRiftStability(float DeltaSeconds);
	void DiscoverSpawnManagers();
	APRSpawnManager* CreateFallbackSpawnManager(APRRiftObjectiveActor* ObjectiveActor);
	APlayerState* ResolveExtractionPlayerState(AController* Controller) const;
	void CountExtractedInventoryItems(int32& OutItemCount, int32& OutUniqueItemTypes) const;
	void ApplyExtractedResourceRules(EPRRiftMissionResult Result, FPRRiftSettlementData& InOutSettlementData);
	void ApplyExtractedResourceRulesForPlayer(APRPlayerState* PlayerState, EPRRiftMissionResult Result, FPRRiftSettlementData& InOutSettlementData, TSet<FName>& UniqueResourceIds) const;
	bool ShouldApplyResourceRulesToPlayer(const APlayerState* PlayerState, EPRRiftMissionResult Result) const;
	static bool IsFallbackMaterialResourceId(FName ItemId);
	void RequestReturnToLobbyTravel(EPRRiftMissionResult Result);
	void PerformReturnToLobbyTravel();
	void TryCompleteSettlementReturn();
	void FinalizePersonalSettlements(EPRRiftMissionResult Result);
	FPRPlayerSettlementReceipt BuildPersonalSettlementReceipt(APRPlayerState* PlayerState, EPRRiftMissionResult Result, const UPRMissionProgressionDataAsset* Mission) const;
	UPRMissionProgressionDataAsset* ResolveMissionContract() const;
	void LogFlowPhase(const TCHAR* Phase, const APlayerState* PlayerState = nullptr) const;
	bool InitializeObjectiveGraph();
	void RefreshObjectiveGraphReplication();
	void SynchronizeObjectiveGraphActors();
	void CacheObjectiveGraphSnapshot();
	void RestoreObjectiveGraphSnapshot();
	void RefreshRiftRuleReplication();
	void ApplyObjectiveStageStabilityCost(EPRObjectiveNodeState PreviousState, FName NodeId);
	void ApplyEnvironmentalRisk();
	void ApplyRulesToConnectedPlayers();
	void UpdateObjectiveItemRecovery();
	bool IsObjectiveItemAvailable(FGuid ItemInstanceGuid) const;

	UPROPERTY(EditDefaultsOnly, Category = "Rift|Diagnostics")
	FName MissionId = FName(TEXT("Mission.Rift.Test.Hold"));

	UPROPERTY(Transient)
	FPRMissionDefinition CurrentMissionDefinition;

	UPROPERTY(Transient)
	TObjectPtr<APRRiftObjectiveActor> ActiveObjective;

	/** Server-only runtime graph. Clients receive only compact GameState summaries. */
	UPROPERTY(VisibleAnywhere, Category = "Rift|ObjectiveGraph")
	TObjectPtr<UPRObjectiveGraphComponent> ObjectiveGraphComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rift|Rules")
	TObjectPtr<UPRRiftRuleComponent> RiftRuleComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rift|Encounter")
	TObjectPtr<UPREncounterDirectorComponent> EncounterDirectorComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APRSpawnManager>> SpawnManagers;

	UPROPERTY(Transient)
	TObjectPtr<APRRescueDroneActor> RescueDrone;

	bool bRiftMissionStarted = false;
	bool bReturnToLobbyTravelPending = false;
	bool bReturnToLobbyServerTravelEnabled = true;
	bool bSettlementFinalizationInProgress = false;
	FGuid CurrentRunId;
	FGuid CurrentSettlementId;
	UPROPERTY(Transient)
	int32 CurrentRunSeed = 0;
	UPROPERTY(Transient)
	TArray<FGuid> EligibleRewardProfileIds;
	/** Boss rewards are server-owned one-shot sources; only accepted defeats may enrich successful extraction rewards. */
	UPROPERTY(Transient)
	TSet<FName> AcceptedBossRewardIds;
	FGuid FinalizedRunId;
	TSet<TObjectKey<APlayerState>> ExtractedPlayerStates;
	TSet<TObjectKey<APREnemyCharacter>> CountedKilledEnemies;
	TSet<TObjectKey<APRPlayerController>> PendingSettlementAcknowledgements;
	UPROPERTY(Transient)
	FPRObjectiveGraphSnapshot ObjectiveGraphSnapshot;
	TMap<FGuid, float> ObjectiveItemMissingSince;
	float NextObjectiveItemRecoveryCheckTime = 0.0f;
	float NextEnvironmentalHazardPulseTime = 0.0f;
	int32 FailedSettlementAcknowledgementCount = 0;
	float SettlementReturnEarliestTime = 0.0f;
	float SettlementAcknowledgementDeadline = 0.0f;
	FTimerHandle ReturnToLobbyTravelTimerHandle;
};
