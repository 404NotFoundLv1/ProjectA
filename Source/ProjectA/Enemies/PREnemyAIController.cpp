#include "Enemies/PREnemyAIController.h"

#include "Characters/PRCharacter.h"
#include "Enemies/PREnemyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

APREnemyAIController::APREnemyAIController()
{
	bAttachToPawn = true;
	PrimaryActorTick.bCanEverTick = true;
}

void APREnemyAIController::BeginPlay()
{
	Super::BeginPlay();
}

void APREnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// Defined enemies are driven by UPREnemyBehaviorComponent timers. The
	// legacy no-definition path remains tick-driven only for backward-compatible placed enemies.
}

void APREnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TargetRefreshTimer);
	GetWorldTimerManager().ClearTimer(LegacyMeleeTimer);
	Super::EndPlay(EndPlayReason);
}

void APREnemyAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetPawn());
	if (!HasAuthority() || !Enemy || Enemy->GetEnemyDefinition())
	{
		return;
	}
	TimeSinceTargetRefresh += DeltaSeconds;
	TimeSinceLastMeleeAttack += DeltaSeconds;
	if (TimeSinceTargetRefresh >= FMath::Max(0.1f, TargetRefreshInterval))
	{
		TimeSinceTargetRefresh = 0.0f;
		RefreshTarget();
	}
	if (TimeSinceLastMeleeAttack >= FMath::Max(0.1f, MeleeAttackInterval))
	{
		TimeSinceLastMeleeAttack = 0.0f;
		TryAttackCurrentTarget();
	}
}

bool APREnemyAIController::RefreshTarget()
{
	if (!HasAuthority())
	{
		return false;
	}

	APRCharacter* NewTarget = FindNearestLivingPlayer();
	CurrentTarget = NewTarget;
	if (!NewTarget)
	{
		StopMovement();
		return false;
	}

	RequestMoveToTarget(NewTarget, AcceptanceRadius);
	return true;
}

void APREnemyAIController::RequestMoveToTarget(APRCharacter* Target, const float InAcceptanceRadius)
{
	if (!HasAuthority() || !Target || !Target->IsAlive())
	{
		StopMovement();
		return;
	}
	CurrentTarget = Target;
	MoveToActor(Target, FMath::Max(0.0f, InAcceptanceRadius), true, true, true, nullptr, true);
}

void APREnemyAIController::StopForEnemyAction()
{
	StopMovement();
}

void APREnemyAIController::HandleTargetRefresh()
{
	if (const APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetPawn()))
	{
		// Definitions are driven by UPREnemyBehaviorComponent. This fallback only
		// preserves movement for legacy placed enemies without a definition.
		if (Enemy->GetEnemyDefinition())
		{
			return;
		}
	}
	RefreshTarget();
}

void APREnemyAIController::HandleLegacyMeleeTimer()
{
	TryAttackCurrentTarget();
}

bool APREnemyAIController::TryAttackCurrentTarget()
{
	if (!HasAuthority())
	{
		return false;
	}
	APREnemyCharacter* Enemy = Cast<APREnemyCharacter>(GetPawn());
	if (!Enemy || Enemy->IsDead() || Enemy->GetEnemyDefinition())
	{
		return false;
	}
	if (!IsValid(CurrentTarget.Get()))
	{
		RefreshTarget();
	}
	return Enemy->TryMeleeAttack(CurrentTarget.Get());
}

APRCharacter* APREnemyAIController::FindNearestLivingPlayer() const
{
	const APawn* ControlledPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return nullptr;
	}

	APRCharacter* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const FVector Origin = ControlledPawn->GetActorLocation();

	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}
	for (const TObjectPtr<APlayerState>& CandidatePlayerState : GameState->PlayerArray)
	{
		APRCharacter* Candidate = CandidatePlayerState ? Cast<APRCharacter>(CandidatePlayerState->GetPawn()) : nullptr;
		if (!Candidate || !Candidate->IsAlive())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}
