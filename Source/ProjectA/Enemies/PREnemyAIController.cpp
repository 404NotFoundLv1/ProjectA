#include "Enemies/PREnemyAIController.h"

#include "Characters/PRCharacter.h"
#include "Enemies/PREnemyCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"

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
	RefreshTarget();
}

void APREnemyAIController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
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

	MoveToActor(NewTarget, AcceptanceRadius, true, true, true, nullptr, true);
	return true;
}

void APREnemyAIController::HandleTargetRefresh()
{
	RefreshTarget();
}

bool APREnemyAIController::TryAttackCurrentTarget()
{
	if (!HasAuthority())
	{
		return false;
	}

	APREnemyCharacter* EnemyPawn = Cast<APREnemyCharacter>(GetPawn());
	if (!EnemyPawn || EnemyPawn->IsDead())
	{
		return false;
	}

	if (!IsValid(CurrentTarget.Get()))
	{
		RefreshTarget();
	}

	return EnemyPawn->TryMeleeAttack(CurrentTarget.Get());
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

	for (TActorIterator<APRCharacter> CharacterIt(World); CharacterIt; ++CharacterIt)
	{
		APRCharacter* Candidate = *CharacterIt;
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
