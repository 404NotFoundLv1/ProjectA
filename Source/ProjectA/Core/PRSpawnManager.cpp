#include "Core/PRSpawnManager.h"

#include "Core/PREnemySpawnPoint.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Enemies/PREnemyCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "Settings/PRProjectSettings.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

APRSpawnManager::APRSpawnManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FClassFinder<APREnemyCharacter> EnemyBlueprintClass(TEXT("/Game/ProjectRift/Blueprints/Enemies/BP_PREnemyCharacter"));
	if (EnemyBlueprintClass.Succeeded())
	{
		SpawnedEnemyClass = EnemyBlueprintClass.Class;
	}
	else
	{
		SpawnedEnemyClass = APREnemyCharacter::StaticClass();
	}
}

void APRSpawnManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		DiscoverSpawnPoints();
	}
}

void APRSpawnManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		StopSpawning();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);
		AliveEnemies.Reset();
		SpawnPoints.Reset();
		ActiveObjective = nullptr;
		NextSpawnPointIndex = 0;
	}

	Super::EndPlay(EndPlayReason);
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

	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while starting the spawn wave timer; using the code default interval."));
	}
	const float SafeWaveInterval = ProjectSettings
		? FMath::Max(0.1f, ProjectSettings->WaveInterval)
		: 6.0f;
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
	for (const TObjectPtr<AActor>& AliveEnemy : AliveEnemies)
	{
		if (IsValid(AliveEnemy))
		{
			AliveEnemy->OnDestroyed.RemoveDynamic(this, &APRSpawnManager::HandleSpawnedActorDestroyed);
		}
	}
	AliveEnemies.Reset();
	SpawnPoints.Reset();
	NextSpawnPointIndex = 0;
}

bool APRSpawnManager::IsWaveTimerActive() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(WaveTimerHandle);
}

int32 APRSpawnManager::SpawnWave()
{
	if (!HasAuthority() || !bSpawningActive)
	{
		return 0;
	}

	PruneDeadEnemies();

	const int32 CurrentAliveCount = GetAliveEnemyCount();
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while calculating the alive enemy cap; using the code default."));
	}
	const int32 MaxAliveEnemies = GetMaxAliveEnemiesForScaling();
	const int32 SpawnBudget = FMath::Max(0, MaxAliveEnemies - CurrentAliveCount);
	const int32 SpawnCount = FMath::Min(GetDesiredEnemiesPerWave(), SpawnBudget);
	if (SpawnCount <= 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UClass* EnemyClass = SpawnedEnemyClass ? SpawnedEnemyClass.Get() : APREnemyCharacter::StaticClass();
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

		const FTransform SpawnTransform = ChooseSpawnTransform();
		AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(EnemyClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!SpawnedActor)
		{
			continue;
		}
		if (APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(SpawnedActor))
		{
			Enemy->SetSpawnHealthMultiplier(GetEnemyHealthMultiplierForScaling());
			const APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>();
			const UPRObjectiveGraphComponent* ObjectiveGraph = RiftGameMode ? RiftGameMode->GetObjectiveGraphComponent() : nullptr;
			const FPRObjectiveNodeDefinition* NodeDefinition = ObjectiveGraph && ActiveObjective
				? ObjectiveGraph->FindNodeDefinition(ActiveObjective->GetObjectiveNodeId())
				: nullptr;
			if (NodeDefinition && NodeDefinition->ObjectiveType == EPRObjectiveType::Hunt)
			{
				Enemy->SetHuntTargetId(NodeDefinition->TargetId);
			}
		}
		SpawnedActor->FinishSpawning(SpawnTransform);

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
	const int32 AlivePlayerCount = FMath::Max(1, GetDifficultyPlayerCountForScaling());
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while calculating enemies per wave; using code defaults."));
	}
	const int32 BaseEnemiesPerWave = ProjectSettings
		? FMath::Max(0, ProjectSettings->BaseEnemiesPerWave)
		: 2;
	const int32 EnemiesPerAdditionalPlayer = ProjectSettings
		? FMath::Max(0, ProjectSettings->EnemiesPerAdditionalPlayer)
		: 1;
	return BaseEnemiesPerWave + FMath::Max(0, AlivePlayerCount - 1) * EnemiesPerAdditionalPlayer;
}

int32 APRSpawnManager::GetDifficultyPlayerCountForScaling() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return 1;
	}

	if (const APRRiftGameState* RiftGameState = Cast<APRRiftGameState>(GameState))
	{
		if (RiftGameState->GetDifficultyPlayerCount() > 0)
		{
			return RiftGameState->GetDifficultyPlayerCount();
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

int32 APRSpawnManager::GetMaxAliveEnemiesForScaling() const
{
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const int32 Base = Settings ? FMath::Max(1, Settings->MaxAliveEnemies) : 8;
	const int32 PerPlayer = Settings ? FMath::Max(0, Settings->MaxAliveEnemiesPerAdditionalPlayer) : 2;
	return Base + FMath::Max(0, GetDifficultyPlayerCountForScaling() - 1) * PerPlayer;
}

float APRSpawnManager::GetEnemyHealthMultiplierForScaling() const
{
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float PerPlayer = Settings && FMath::IsFinite(Settings->EnemyHealthMultiplierPerAdditionalPlayer)
		? FMath::Clamp(Settings->EnemyHealthMultiplierPerAdditionalPlayer, 0.0f, 10.0f)
		: 0.25f;
	return 1.0f + FMath::Max(0, GetDifficultyPlayerCountForScaling() - 1) * PerPlayer;
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
