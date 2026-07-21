#pragma once

#include "CoreMinimal.h"
#include "Core/PREncounterDirectorTypes.h"
#include "GameFramework/Actor.h"
#include "PRSpawnManager.generated.h"

class APREnemySpawnPoint;
class APRRiftObjectiveActor;
class APREncounterExclusionVolume;
class UPREnemyRosterDataAsset;

/**
 * Server-authoritative wave spawner for rift objectives.
 */
UCLASS()
class PROJECTA_API APRSpawnManager : public AActor
{
	GENERATED_BODY()

public:
	APRSpawnManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	bool StartSpawningForObjective(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	void StopSpawning();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	int32 SpawnWave();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Spawning")
	int32 DiscoverSpawnPoints();

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	int32 GetAliveEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	int32 GetSpawnedWaveCount() const { return SpawnedWaveCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	bool IsSpawningActive() const { return bSpawningActive; }

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	bool IsWaveTimerActive() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	int32 GetDesiredEnemiesPerWave() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	int32 GetDifficultyPlayerCountForScaling() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	int32 GetMaxAliveEnemiesForScaling() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Spawning")
	float GetEnemyHealthMultiplierForScaling() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") FName GetEncounterRegionId() const { return EncounterRegionId; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Encounter") int32 ExecuteEncounterSpawnRequest(const FPREncounterSpawnRequest& Request);
	/** Server-only helper used by the Summoner action. Summons use the same caps and threat accounting as director spawns. */
	int32 SpawnSummonedEnemies(FName DefinitionId, int32 RequestedCount);
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") float GetAliveEncounterThreat() const;
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") int32 GetAliveEncounterCategoryCount(EPREncounterUnitCategory Category) const;
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") int32 GetAliveEnemyDefinitionCount(FName EnemyDefinitionId) const;
	const TArray<FPREncounterSpawnEntry>& GetEncounterSpawnEntries() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Spawning")
	TSubclassOf<AActor> SpawnedEnemyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Encounter") FName EncounterRegionId = FName(TEXT("Region.Default"));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Encounter") TObjectPtr<UPREnemyRosterDataAsset> EnemyRoster;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Encounter") TArray<FPREncounterSpawnEntry> EncounterSpawnEntries;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rift|Spawning")
	bool bSpawningActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rift|Spawning")
	int32 SpawnedWaveCount = 0;

private:
	void HandleWaveTimerElapsed();
	void PruneDeadEnemies() const;
	void BuildRuntimeRosterEntries();
	bool ChooseEncounterSpawnTransform(const FPREncounterSpawnRequest& Request, FTransform& OutTransform, FString& OutRejectionReason) const;
	const struct FPREnemyRosterEntry* FindRosterEntry(FName DefinitionId) const;

	UFUNCTION()
	void HandleSpawnedActorDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<APRRiftObjectiveActor> ActiveObjective;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APREnemySpawnPoint>> SpawnPoints;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<AActor>> AliveEnemies;
	UPROPERTY(Transient) TArray<FPREncounterSpawnEntry> RuntimeRosterSpawnEntries;
	TMap<TWeakObjectPtr<AActor>, FPREncounterSpawnRequest> EncounterUnits;
	UPROPERTY(Transient) TArray<TObjectPtr<APREncounterExclusionVolume>> EncounterExclusionVolumes;

	UPROPERTY(Transient)
	int32 NextSpawnPointIndex = 0;

	FTimerHandle WaveTimerHandle;
};
