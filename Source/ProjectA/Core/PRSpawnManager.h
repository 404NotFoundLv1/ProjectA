#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRSpawnManager.generated.h"

class APREnemySpawnPoint;
class APRRiftObjectiveActor;

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

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Spawning")
	TSubclassOf<AActor> SpawnedEnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Spawning", meta = (ClampMin = "0"))
	int32 BaseEnemiesPerWave = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Spawning", meta = (ClampMin = "0"))
	int32 EnemiesPerAdditionalPlayer = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Spawning", meta = (ClampMin = "1"))
	int32 MaxAliveEnemies = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rift|Spawning", meta = (ClampMin = "0.1"))
	float WaveInterval = 6.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rift|Spawning")
	bool bSpawningActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rift|Spawning")
	int32 SpawnedWaveCount = 0;

private:
	void HandleWaveTimerElapsed();
	int32 GetDesiredEnemiesPerWave() const;
	int32 GetAlivePlayerCountForScaling() const;
	void PruneDeadEnemies() const;
	FTransform ChooseSpawnTransform();
	void EnsureFallbackSpawnPoint();

	UFUNCTION()
	void HandleSpawnedActorDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<APRRiftObjectiveActor> ActiveObjective;

	UPROPERTY(Transient)
	TArray<TObjectPtr<APREnemySpawnPoint>> SpawnPoints;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<AActor>> AliveEnemies;

	UPROPERTY(Transient)
	int32 NextSpawnPointIndex = 0;

	FTimerHandle WaveTimerHandle;
};
