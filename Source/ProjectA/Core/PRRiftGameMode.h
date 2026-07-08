#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameModeBase.h"
#include "Core/PRRiftGameState.h"
#include "UObject/ObjectKey.h"
#include "PRRiftGameMode.generated.h"

class APRRiftObjectiveActor;
class APRSpawnManager;
class APlayerState;

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
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

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

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	FString GetReturnToLobbyMapPath() const { return ReturnToLobbyMapPath; }

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	FString BuildReturnToLobbyTravelURL() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	bool IsReturnToLobbyTravelPending() const { return bReturnToLobbyTravelPending; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Extraction")
	void SetReturnToLobbyServerTravelEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	int32 GetExtractedPlayerCount() const { return ExtractedPlayerStates.Num(); }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void UpdateAlivePlayerCount();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveActivated(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveProgressChanged(APRRiftObjectiveActor* ObjectiveActor, float ObjectiveProgress);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	void HandleObjectiveCompleted(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	bool StartSpawnManagersForObjective(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	void StopSpawnManagers();

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	bool HasRiftMissionStarted() const { return bRiftMissionStarted; }

protected:
	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	APRRiftGameState* GetRiftGameState() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	int32 CountAlivePlayers() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Mission", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialRiftStability = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Extraction")
	FString ReturnToLobbyMapPath = TEXT("/Game/ProjectRift/Maps/L_ShipLobby");

private:
	void DiscoverSpawnManagers();
	APRSpawnManager* CreateFallbackSpawnManager(APRRiftObjectiveActor* ObjectiveActor);
	APlayerState* ResolveExtractionPlayerState(AController* Controller) const;
	void RequestReturnToLobbyTravel();

	UPROPERTY(Transient)
	TObjectPtr<APRRiftObjectiveActor> ActiveObjective;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APRSpawnManager>> SpawnManagers;

	bool bRiftMissionStarted = false;
	bool bReturnToLobbyTravelPending = false;
	bool bReturnToLobbyServerTravelEnabled = true;
	TSet<TObjectKey<APlayerState>> ExtractedPlayerStates;
};
