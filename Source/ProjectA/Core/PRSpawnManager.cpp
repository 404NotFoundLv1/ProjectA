#include "Core/PRSpawnManager.h"

#include "Core/PREnemySpawnPoint.h"
#include "Core/PREncounterExclusionVolume.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftRuleComponent.h"
#include "Core/PRGameplayTags.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Enemies/PREnemyCharacter.h"
#include "Enemies/PREnemyRosterDataAsset.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Characters/PRCharacter.h"
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
		BuildRuntimeRosterEntries();
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
	// v0.8.3: cadence is owned by the GameMode encounter director. This method
	// remains the legacy activation adapter used by Blueprint callers.
	UE_LOG(LogProjectA, Log, TEXT("Rift spawn manager registered with encounter director. Manager=%s SpawnPoints=%d"), *GetNameSafe(this), SpawnPoints.Num());

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
	EncounterUnits.Reset();
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

	int32 SpawnedCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
	{
		FPREncounterSpawnRequest Request;
		Request.EntryId = TEXT("Enemy.Basic");
		Request.Category = EPREncounterUnitCategory::Melee;
		Request.ThreatCost = 1.0f;
		Request.ObjectiveNodeId = ActiveObjective ? ActiveObjective->GetObjectiveNodeId() : NAME_None;
		SpawnedCount += ExecuteEncounterSpawnRequest(Request);
	}

	if (SpawnedCount > 0)
	{
		UE_LOG(LogProjectA, Log, TEXT("Rift spawn wave spawned. Manager=%s Requests=%d Spawned=%d Alive=%d"),
			*GetNameSafe(this),
			SpawnedWaveCount,
			SpawnedCount,
			GetAliveEnemyCount());
	}

	return SpawnedCount;
}

int32 APRSpawnManager::ExecuteEncounterSpawnRequest(const FPREncounterSpawnRequest& Request)
{
	if (!HasAuthority() || !bSpawningActive || Request.ThreatCost <= 0.0f)
	{
		return 0;
	}
	if (Request.PackSize > 1)
	{
		FPREncounterSpawnRequest SingleRequest = Request;
		SingleRequest.PackSize = 1;
		int32 Spawned = 0;
		for (int32 Index = 0; Index < Request.PackSize && GetAliveEnemyCount() < GetMaxAliveEnemiesForScaling(); ++Index)
		{
			Spawned += ExecuteEncounterSpawnRequest(SingleRequest);
		}
		return Spawned;
	}
	FPREncounterSpawnRequest ResolvedRequest = Request;
	const FName RequestedDefinitionId = !Request.EnemyDefinitionId.IsNone() ? Request.EnemyDefinitionId : Request.EntryId;
	const FPREnemyRosterEntry* RosterEntry = FindRosterEntry(RequestedDefinitionId);
	if (!Request.EnemyDefinitionId.IsNone() && !RosterEntry)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Encounter spawn rejected unknown enemy definition '%s'."), *Request.EnemyDefinitionId.ToString());
		return 0;
	}
	if (RosterEntry && RosterEntry->Definition)
	{
		ResolvedRequest.EnemyDefinitionId = RosterEntry->Definition->EnemyId;
		ResolvedRequest.EntryId = RosterEntry->Definition->EnemyId;
		ResolvedRequest.ThreatCost = RosterEntry->Definition->ThreatCost;
		ResolvedRequest.Category = RosterEntry->Definition->EncounterCategory;
		ResolvedRequest.bElite = RosterEntry->Definition->Tier == EPREnemyTier::Elite;
	}
	UWorld* World = GetWorld();
	UClass* EnemyClass = RosterEntry && RosterEntry->EnemyClass ? RosterEntry->EnemyClass.Get()
		: (Request.EnemyClass ? Request.EnemyClass.Get() : (SpawnedEnemyClass ? SpawnedEnemyClass.Get() : APREnemyCharacter::StaticClass()));
	FTransform SpawnTransform;
	FString RejectionReason;
	if (!World || !EnemyClass || !ChooseEncounterSpawnTransform(ResolvedRequest, SpawnTransform, RejectionReason))
	{
		return 0;
	}
	AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(EnemyClass, SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!SpawnedActor) return 0;
	if (APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(SpawnedActor))
	{
		if (RosterEntry && !Enemy->SetEnemyDefinition(RosterEntry->Definition))
		{
			SpawnedActor->Destroy();
			return 0;
		}
		Enemy->SetSpawnHealthMultiplier(GetEnemyHealthMultiplierForScaling());
		if (APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>())
		{
			if (ResolvedRequest.bElite && RiftGameMode->GetRiftRuleComponent())
			{
				const FPRMissionRuleSnapshot Rules = RiftGameMode->GetRiftRuleComponent()->GetRuleSnapshot();
				Enemy->SetSpawnHealthMultiplier(GetEnemyHealthMultiplierForScaling() * Rules.EliteHealthMultiplier);
				Enemy->SetSpawnAttackPowerMultiplier(Rules.EliteAttackMultiplier);
			}
			if (const UPRObjectiveGraphComponent* Graph = RiftGameMode->GetObjectiveGraphComponent())
			{
				if (const FPRObjectiveNodeDefinition* Node = ActiveObjective ? Graph->FindNodeDefinition(ActiveObjective->GetObjectiveNodeId()) : nullptr)
					if (Node->ObjectiveType == EPRObjectiveType::Hunt) Enemy->SetHuntTargetId(Node->TargetId);
			}
		}
	}
	SpawnedActor->FinishSpawning(SpawnTransform);
	if (APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(SpawnedActor))
	{
		if (APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>())
			if (UPRRiftRuleComponent* Rules = RiftGameMode->GetRiftRuleComponent()) Rules->ApplyRulesToEnemy(Enemy);
		if (ResolvedRequest.bElite)
			if (UPRAbilitySystemComponent* AbilitySystem = Enemy->GetProjectRiftAbilitySystemComponent()) AbilitySystem->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Enemy_Elite);
	}
	SpawnedActor->SetReplicates(true);
	SpawnedActor->SetReplicateMovement(true);
	SpawnedActor->OnDestroyed.AddDynamic(this, &APRSpawnManager::HandleSpawnedActorDestroyed);
	AliveEnemies.Add(SpawnedActor);
	EncounterUnits.Add(SpawnedActor, ResolvedRequest);
	++SpawnedWaveCount;
	return 1;
}

const TArray<FPREncounterSpawnEntry>& APRSpawnManager::GetEncounterSpawnEntries() const
{
	return RuntimeRosterSpawnEntries.IsEmpty() ? EncounterSpawnEntries : RuntimeRosterSpawnEntries;
}

void APRSpawnManager::BuildRuntimeRosterEntries()
{
	RuntimeRosterSpawnEntries.Reset();
	if (!EnemyRoster)
	{
		return;
	}
	FString Diagnostic;
	if (!EnemyRoster->ValidateRoster(Diagnostic))
	{
		UE_LOG(LogProjectA, Error, TEXT("Enemy roster '%s' rejected: %s"), *GetNameSafe(EnemyRoster), *Diagnostic);
		return;
	}
	for (const FPREnemyRosterEntry& Entry : EnemyRoster->Entries)
	{
		FPREncounterSpawnEntry& RuntimeEntry = RuntimeRosterSpawnEntries.AddDefaulted_GetRef();
		RuntimeEntry.EntryId = Entry.Definition->EnemyId;
		RuntimeEntry.EnemyDefinitionId = Entry.Definition->EnemyId;
		RuntimeEntry.EnemyClass = Entry.EnemyClass;
		RuntimeEntry.ThreatCost = Entry.Definition->ThreatCost;
		RuntimeEntry.Category = Entry.Definition->EncounterCategory;
		RuntimeEntry.SelectionWeight = Entry.SelectionWeight;
		RuntimeEntry.PackSize = Entry.PackSize;
		RuntimeEntry.MinimumFrozenPlayerCount = Entry.MinimumFrozenPlayerCount;
		RuntimeEntry.MaxAlive = Entry.MaxAlive;
		RuntimeEntry.MinimumRiskTier = Entry.MinimumRiskTier;
	}
}

const FPREnemyRosterEntry* APRSpawnManager::FindRosterEntry(const FName DefinitionId) const
{
	if (!EnemyRoster || DefinitionId.IsNone())
	{
		return nullptr;
	}
	for (const FPREnemyRosterEntry& Entry : EnemyRoster->Entries)
	{
		if (Entry.Definition && Entry.Definition->EnemyId == DefinitionId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

int32 APRSpawnManager::SpawnSummonedEnemies(const FName DefinitionId, const int32 RequestedCount, AActor* SummonOwner)
{
	if (!HasAuthority() || DefinitionId.IsNone() || RequestedCount <= 0)
	{
		return 0;
	}
	int32 ExistingSummons = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FPREncounterSpawnRequest>& Pair : EncounterUnits)
	{
		if (Pair.Key.IsValid() && Pair.Value.bSummoned) ++ExistingSummons;
	}
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Min3(RequestedCount, 6 - ExistingSummons, 6); ++Index)
	{
		if (GetAliveEnemyCount() >= GetMaxAliveEnemiesForScaling()) break;
		FPREncounterSpawnRequest Request;
		Request.EntryId = DefinitionId;
		Request.EnemyDefinitionId = DefinitionId;
		Request.ThreatCost = 0.5f;
		Request.Category = EPREncounterUnitCategory::Melee;
		Request.bSummoned = true;
		Request.ObjectiveNodeId = ActiveObjective ? ActiveObjective->GetObjectiveNodeId() : NAME_None;
		const int32 SpawnResult = ExecuteEncounterSpawnRequest(Request);
		Spawned += SpawnResult;
		if (SpawnResult > 0 && SummonOwner && !AliveEnemies.IsEmpty() && IsValid(AliveEnemies.Last()))
		{
			AliveEnemies.Last()->SetOwner(SummonOwner);
		}
	}
	return Spawned;
}

float APRSpawnManager::GetAliveEncounterThreat() const
{
	PruneDeadEnemies();
	float Threat = 0.0f;
	for (const TPair<TWeakObjectPtr<AActor>, FPREncounterSpawnRequest>& Pair : EncounterUnits) if (Pair.Key.IsValid()) Threat += FMath::Max(0.0f, Pair.Value.ThreatCost);
	return Threat;
}

int32 APRSpawnManager::GetAliveEncounterCategoryCount(const EPREncounterUnitCategory Category) const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FPREncounterSpawnRequest>& Pair : EncounterUnits) if (Pair.Key.IsValid() && Pair.Value.Category == Category) ++Count;
	return Count;
}

int32 APRSpawnManager::GetAliveEnemyDefinitionCount(const FName EnemyDefinitionId) const
{
	if (EnemyDefinitionId.IsNone())
	{
		return 0;
	}
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FPREncounterSpawnRequest>& Pair : EncounterUnits)
	{
		if (Pair.Key.IsValid() && (Pair.Value.EnemyDefinitionId == EnemyDefinitionId || Pair.Value.EntryId == EnemyDefinitionId))
		{
			++Count;
		}
	}
	return Count;
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
	EncounterExclusionVolumes.Reset();
	for (TActorIterator<APREncounterExclusionVolume> VolumeIt(World); VolumeIt; ++VolumeIt)
	{
		if (APREncounterExclusionVolume* Volume = *VolumeIt) EncounterExclusionVolumes.Add(Volume);
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
	const int32 BaseCount = BaseEnemiesPerWave + FMath::Max(0, AlivePlayerCount - 1) * EnemiesPerAdditionalPlayer;
	const APRRiftGameState* RiftGameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
	const float Multiplier = RiftGameState ? RiftGameState->GetRiftRiskSnapshot().EnemyBudgetMultiplier : 1.0f;
	return FMath::Max(0, FMath::RoundToInt(BaseCount * Multiplier));
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
	// This is a safety cap, not a threat target: risk may raise pressure but
	// never permits more actors than the frozen-party hard limit.
	return FMath::Max(1, Base + FMath::Max(0, GetDifficultyPlayerCountForScaling() - 1) * PerPlayer);
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
	for (auto It = const_cast<APRSpawnManager*>(this)->EncounterUnits.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
}

bool APRSpawnManager::ChooseEncounterSpawnTransform(const FPREncounterSpawnRequest& Request, FTransform& OutTransform, FString& OutRejectionReason) const
{
	UWorld* World = GetWorld();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float MinimumDistance = Settings ? Settings->EncounterMinimumPlayerDistance : 1000.0f;
	const int32 MaximumAttempts = Settings ? FMath::Max(1, Settings->EncounterMaximumCandidateAttempts) : 8;
	const float NavigationRadius = Settings ? FMath::Max(0.0f, Settings->EncounterNavProjectionRadius) : 250.0f;
	if (!World)
	{
		OutRejectionReason = TEXT("World is unavailable.");
		return false;
	}
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		OutRejectionReason = TEXT("Navigation system is unavailable.");
		return false;
	}
	TArray<const APREnemySpawnPoint*> Candidates;
	for (const APREnemySpawnPoint* Point : SpawnPoints)
	{
		if (IsValid(Point) && Point->AllowsObjectiveNode(Request.ObjectiveNodeId) && (Request.SpawnGroupId.IsNone() || Point->GetSpawnGroupId() == Request.SpawnGroupId)) Candidates.Add(Point);
	}
	Candidates.Sort([](const APREnemySpawnPoint& Left, const APREnemySpawnPoint& Right) { return Left.GetPathName() < Right.GetPathName(); });
	FRandomStream Random(HashCombineFast(HashCombineFast(::GetTypeHash(Request.RunSeed), Request.ObjectiveNodeId.GetComparisonIndex().ToUnstableInt()), ::GetTypeHash(Request.DecisionOrdinal)));
	for (int32 Attempt = 0; Attempt < MaximumAttempts && !Candidates.IsEmpty(); ++Attempt)
	{
		float TotalWeight = 0.0f;
		for (const APREnemySpawnPoint* Point : Candidates) TotalWeight += FMath::Max(0.01f, Point->GetEncounterWeight());
		float Pick = Random.FRandRange(0.0f, TotalWeight);
		int32 SelectedIndex = Candidates.Num() - 1;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index) { Pick -= FMath::Max(0.01f, Candidates[Index]->GetEncounterWeight()); if (Pick <= 0.0f) { SelectedIndex = Index; break; } }
		const APREnemySpawnPoint* Point = Candidates[SelectedIndex];
		Candidates.RemoveAtSwap(SelectedIndex, EAllowShrinking::No);
		FNavLocation ProjectedLocation;
		if (!NavigationSystem->ProjectPointToNavigation(Point->GetActorLocation(), ProjectedLocation, FVector(NavigationRadius)))
		{
			continue;
		}
		const FVector Candidate = ProjectedLocation.Location;
		constexpr float EnemyCapsuleRadius = 42.0f;
		constexpr float EnemyCapsuleHalfHeight = 90.0f;
		const FVector CapsuleCenter = ProjectRiftEncounterSpawn::GetCapsuleCenterForNavigationLocation(Candidate, EnemyCapsuleHalfHeight);
		bool bSafe = true;
		for (const APREncounterExclusionVolume* Volume : EncounterExclusionVolumes)
		{
			if (IsValid(Volume) && Volume->ExcludesLocation(Candidate)) { bSafe = false; break; }
		}
		if (World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(EnemyCapsuleRadius, EnemyCapsuleHalfHeight))
			|| World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(EnemyCapsuleRadius, EnemyCapsuleHalfHeight)))
		{
			bSafe = false;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); bSafe && It; ++It)
		{
			const APlayerController* Controller = It->Get();
			const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			if (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), Candidate) < FMath::Square(MinimumDistance)) { bSafe = false; break; }
			if (Controller && Pawn && Settings && FVector::Dist(Pawn->GetActorLocation(), Candidate) <= Settings->EncounterVisibilityCheckDistance)
			{
				FVector ViewLocation; FRotator ViewRotation;
				Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
				const FVector ToCandidate = (Candidate - ViewLocation).GetSafeNormal();
				const float MinDot = FMath::Cos(FMath::DegreesToRadians(Settings->EncounterVisibilityConeHalfAngleDegrees));
				FHitResult Hit;
				if (FVector::DotProduct(ViewRotation.Vector(), ToCandidate) >= MinDot && !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Candidate, ECC_Visibility)) { bSafe = false; break; }
			}
		}
		if (bSafe) { OutTransform = FTransform(Point->GetActorRotation(), CapsuleCenter, Point->GetActorScale3D()); return true; }
	}
	OutRejectionReason = TEXT("No configured spawn point passed the bounded navigation, collision, exclusion, distance, and visibility checks.");
	return false;
}

void APRSpawnManager::HandleSpawnedActorDestroyed(AActor* DestroyedActor)
{
	AliveEnemies.Remove(DestroyedActor);
	EncounterUnits.Remove(DestroyedActor);
}
