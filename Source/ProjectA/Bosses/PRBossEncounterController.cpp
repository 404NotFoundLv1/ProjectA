#include "Bosses/PRBossEncounterController.h"

#include "Bosses/PRBossCharacter.h"
#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Bosses/PRBossSchedulerComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/PRRiftGameState.h"
#include "Characters/PRCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "ProjectA.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

APRBossEncounterController::APRBossEncounterController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void APRBossEncounterController::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bAutoStart)
	{
		if (AutoStartDelaySeconds <= 0.0f)
		{
			StartAutoBossEncounter();
		}
		else
		{
			GetWorldTimerManager().SetTimer(AutoStartTimer, this, &APRBossEncounterController::StartAutoBossEncounter, AutoStartDelaySeconds, false);
		}
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RecoveryTimer, this, &APRBossEncounterController::MonitorRecovery, 1.0f, true);
	}
}

void APRBossEncounterController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutoStartTimer);
	GetWorldTimerManager().ClearTimer(RecoveryTimer);
	Super::EndPlay(EndPlayReason);
}

void APRBossEncounterController::StartAutoBossEncounter()
{
	if (HasAuthority() && bAutoStart && !ActiveBoss)
	{
		StartBossEncounter();
	}
}

bool APRBossEncounterController::StartBossEncounter()
{
	if (!HasAuthority() || ActiveBoss || !BossDefinition)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Boss encounter start rejected. Authority=%d ActiveBoss=%s Definition=%s"), HasAuthority(), *GetNameSafe(ActiveBoss), *GetNameSafe(BossDefinition));
		return false;
	}

	FString Diagnostic;
	if (!BossDefinition->ValidateDefinition(Diagnostic))
	{
		UE_LOG(LogProjectA, Error, TEXT("Boss encounter definition is invalid: %s"), *Diagnostic);
		return false;
	}
	TSubclassOf<APRBossCharacter> SpawnClass = BossClass ? BossClass : BossDefinition->BossClass;
	if (!SpawnClass)
	{
		SpawnClass = APRBossCharacter::StaticClass();
	}

	const int32 FrozenPlayerCount = CountConnectedPlayers();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APRBossCharacter* SpawnedBoss = GetWorld()->SpawnActorDeferred<APRBossCharacter>(SpawnClass, BossAnchor, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!SpawnedBoss || !SpawnedBoss->InitializeBoss(BossDefinition, this, FrozenPlayerCount))
	{
		UE_LOG(LogProjectA, Error, TEXT("Boss encounter failed to spawn or initialize Boss=%s Definition=%s"), *GetNameSafe(SpawnedBoss), *GetNameSafe(BossDefinition));
		if (SpawnedBoss) { SpawnedBoss->Destroy(); }
		return false;
	}
	ActiveBoss = SpawnedBoss;
	SpawnedBoss->FinishSpawning(BossAnchor);
	UE_LOG(LogProjectA, Log, TEXT("Boss encounter started. Boss=%s Definition=%s Players=%d"), *GetNameSafe(ActiveBoss), *GetNameSafe(BossDefinition), FrozenPlayerCount);
	PendingRewardId = NAME_None;
	return true;
}

void APRBossEncounterController::ResetBossEncounter()
{
	PendingRewardId = NAME_None;
	if (ActiveBoss)
	{
		if (UAbilitySystemComponent* AbilitySystem = ActiveBoss->GetAbilitySystemComponent())
		{
			AbilitySystem->CancelAllAbilities();
			UPRCombatEffectLibrary::ClearNegativeStatusEffects(AbilitySystem);
		}
		ActiveBoss->Destroy();
		ActiveBoss = nullptr;
	}
	if (APRRiftGameState* RiftGameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr)
	{
		RiftGameState->SetBossRuntimeSnapshot(FPRBossRuntimeSnapshot());
	}
}

void APRBossEncounterController::AbortBossEncounter()
{
	ResetBossEncounter();
}

void APRBossEncounterController::HandleEncounterWipe()
{
	if (!HasAuthority() || !bResetOnFullWipe || !ActiveBoss || PendingRewardId != NAME_None) { return; }
	ResetBossEncounter();
}

void APRBossEncounterController::NotifyBossDefeated(APRBossCharacter* DefeatedBoss)
{
	if (!HasAuthority() || DefeatedBoss != ActiveBoss || PendingRewardId != NAME_None || !BossDefinition)
	{
		return;
	}
	PendingRewardId = BossDefinition->RewardId;
	if (APRRiftGameState* RiftGameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr)
	{
		FPRBossRuntimeSnapshot Snapshot = DefeatedBoss->GetBossScheduler()
			? DefeatedBoss->GetBossScheduler()->GetRuntimeSnapshot()
			: FPRBossRuntimeSnapshot();
		Snapshot.BossId = BossDefinition->BossId;
		Snapshot.State = EPRBossRuntimeState::Defeated;
		Snapshot.bVisible = true;
		Snapshot.bInterruptible = false;
		Snapshot.bStaggered = false;
		Snapshot.ActivePatternId = NAME_None;
		RiftGameState->SetBossRuntimeSnapshot(Snapshot);
	}
}

void APRBossEncounterController::RecoverBossToAnchor()
{
	if (ActiveBoss)
	{
		ActiveBoss->SetActorTransform(BossAnchor);
	}
}

bool APRBossEncounterController::HasConnectedPlayer() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState && GameState->PlayerArray.ContainsByPredicate([](const APlayerState* PlayerState)
	{
		return PlayerState && !PlayerState->IsOnlyASpectator();
	});
}

int32 APRBossEncounterController::CountConnectedPlayers() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	int32 ConnectedPlayerCount = 0;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && !PlayerState->IsOnlyASpectator())
			{
				++ConnectedPlayerCount;
			}
		}
	}
	return FMath::Clamp(ConnectedPlayerCount, 1, 4);
}

bool APRBossEncounterController::HasLivingPlayer() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GameState && GameState->PlayerArray.ContainsByPredicate([](const APlayerState* PlayerState)
	{
		const APRCharacter* Character = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
		return Character && Character->IsAlive();
	});
}

void APRBossEncounterController::MonitorRecovery()
{
	if (!HasAuthority() || !ActiveBoss || PendingRewardId != NAME_None) { return; }
	if (ArenaRecoveryRadius > 0.0f && FVector::DistSquared(ActiveBoss->GetActorLocation(), BossAnchor.GetLocation()) > FMath::Square(ArenaRecoveryRadius))
	{
		RecoverBossToAnchor();
	}
	if (!HasConnectedPlayer())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (NoPlayerSince < 0.0f) { NoPlayerSince = Now; }
		if (Now - NoPlayerSince >= DisconnectGraceSeconds) { ResetBossEncounter(); }
		return;
	}
	NoPlayerSince = -1.0f;
	if (!HasLivingPlayer()) { HandleEncounterWipe(); }
}

FName APRBossEncounterController::ConsumePendingRewardId()
{
	const FName Result = PendingRewardId;
	PendingRewardId = NAME_None;
	return Result;
}
