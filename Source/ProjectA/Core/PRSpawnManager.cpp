#include "Core/PRSpawnManager.h"

#include "Core/PREnemySpawnPoint.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRRiftObjectiveActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "TimerManager.h"

APRSpawnManager::APRSpawnManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SpawnedEnemyClass = ADefaultPawn::StaticClass();
}

void APRSpawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		DiscoverSpawnPoints();
	}
}

void APRSpawnManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRSpawnManager, bSpawningActive);
	DOREPLIFETIME(APRSpawnManager, SpawnedWaveCount);
}

bool APRSpawnManager::StartSpawningForObjective(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority())
	{
		return false;
	}

	ActiveObjective = ObjectiveActor;
	bSpawningActive = true;
	DiscoverSpawnPoints();
	SpawnWave();

	const float SafeWaveInterval = FMath::Max(0.1f, WaveInterval);
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &APRSpawnManager::HandleWaveTimerElapsed, SafeWaveInterval, true);

	UE_LOG(LogProjectA, Log, TEXT("Rift spawn manager started. Manager=%s WaveInterval=%.1f SpawnPoints=%d"),
		*GetNameSafe(this),
		SafeWaveInterval,
		SpawnPoints.Num());

	return true;
}

void APRSpawnManager::StopSpawning()
{
	if (!HasAuthority())
	{
		return;
	}

	bSpawningActive = false;
	ActiveObjective = nullptr;
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
}

int32 APRSpawnManager::SpawnWave()
{
	if (!HasAuthority() || !bSpawningActive)
	{
		return 0;
	}

	PruneDeadEnemies();

	const int32 CurrentAliveCount = GetAliveEnemyCount();
	const int32 SpawnBudget = FMath::Max(0, MaxAliveEnemies - CurrentAliveCount);
	const int32 SpawnCount = FMath::Min(GetDesiredEnemiesPerWave(), SpawnBudget);
	if (SpawnCount <= 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UClass* EnemyClass = SpawnedEnemyClass ? SpawnedEnemyClass.Get() : ADefaultPawn::StaticClass();
	if (!World || !EnemyClass)
	{
		return 0;
	}

	int32 SpawnedCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedActor = World->SpawnActor<AActor>(EnemyClass, ChooseSpawnTransform(), SpawnParameters);
		if (!SpawnedActor)
		{
			continue;
		}

		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
		SpawnedActor->OnDestroyed.AddDynamic(this, &APRSpawnManager::HandleSpawnedActorDestroyed);
		AliveEnemies.Add(SpawnedActor);
		++SpawnedCount;
	}

	if (SpawnedCount > 0)
	{
		++SpawnedWaveCount;
		UE_LOG(LogProjectA, Log, TEXT("Rift spawn wave spawned. Manager=%s Wave=%d Spawned=%d Alive=%d"),
			*GetNameSafe(this),
			SpawnedWaveCount,
			SpawnedCount,
			GetAliveEnemyCount());
	}

	return SpawnedCount;
}

int32 APRSpawnManager::DiscoverSpawnPoints()
{
	SpawnPoints.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	for (TActorIterator<APREnemySpawnPoint> SpawnPointIt(World); SpawnPointIt; ++SpawnPointIt)
	{
		if (APREnemySpawnPoint* SpawnPoint = *SpawnPointIt)
		{
			SpawnPoints.Add(SpawnPoint);
		}
	}

	if (SpawnPoints.IsEmpty())
	{
		EnsureFallbackSpawnPoint();
	}

	return SpawnPoints.Num();
}

int32 APRSpawnManager::GetAliveEnemyCount() const
{
	PruneDeadEnemies();
	return AliveEnemies.Num();
}

void APRSpawnManager::HandleWaveTimerElapsed()
{
	SpawnWave();
}

int32 APRSpawnManager::GetDesiredEnemiesPerWave() const
{
	const int32 AlivePlayerCount = FMath::Max(1, GetAlivePlayerCountForScaling());
	return FMath::Max(0, BaseEnemiesPerWave) + FMath::Max(0, AlivePlayerCount - 1) * FMath::Max(0, EnemiesPerAdditionalPlayer);
}

int32 APRSpawnManager::GetAlivePlayerCountForScaling() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return 1;
	}

	if (const APRRiftGameState* RiftGameState = Cast<APRRiftGameState>(GameState))
	{
		if (RiftGameState->GetAlivePlayerCount() > 0)
		{
			return RiftGameState->GetAlivePlayerCount();
		}
	}

	int32 PlayerCount = 0;
	for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
	{
		if (PlayerState && !PlayerState->IsOnlyASpectator())
		{
			++PlayerCount;
		}
	}

	return FMath::Max(1, PlayerCount);
}

void APRSpawnManager::PruneDeadEnemies() const
{
	AliveEnemies.RemoveAll([](const TObjectPtr<AActor>& Candidate)
	{
		return !IsValid(Candidate);
	});
}

FTransform APRSpawnManager::ChooseSpawnTransform()
{
	if (SpawnPoints.IsEmpty())
	{
		EnsureFallbackSpawnPoint();
	}

	if (!SpawnPoints.IsEmpty())
	{
		const int32 SafeIndex = NextSpawnPointIndex % SpawnPoints.Num();
		NextSpawnPointIndex = (NextSpawnPointIndex + 1) % SpawnPoints.Num();
		if (const APREnemySpawnPoint* SpawnPoint = SpawnPoints[SafeIndex])
		{
			return SpawnPoint->GetSpawnTransform();
		}
	}

	return FTransform(GetActorRotation(), GetActorLocation() + FVector(600.0f, 0.0f, 60.0f));
}

void APRSpawnManager::EnsureFallbackSpawnPoint()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = ActiveObjective ? ActiveObjective->GetActorLocation() : GetActorLocation();
	const FVector FallbackLocation = Origin + FVector(650.0f, 0.0f, 60.0f);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APREnemySpawnPoint* FallbackSpawnPoint = World->SpawnActor<APREnemySpawnPoint>(
		APREnemySpawnPoint::StaticClass(),
		FallbackLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (FallbackSpawnPoint)
	{
		SpawnPoints.Add(FallbackSpawnPoint);
	}
}

void APRSpawnManager::HandleSpawnedActorDestroyed(AActor* DestroyedActor)
{
	AliveEnemies.Remove(DestroyedActor);
}
