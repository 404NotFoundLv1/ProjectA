#include "Core/PRHoldObjectiveActor.h"

#include "Net/UnrealNetwork.h"

APRHoldObjectiveActor::APRHoldObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APRHoldObjectiveActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsObjectiveActive())
	{
		return;
	}

	const float SafeHoldDuration = FMath::Max(0.1f, HoldDuration);
	SetCurrentHoldTime(CurrentHoldTime + DeltaSeconds);
	SetObjectiveProgress(CurrentHoldTime / SafeHoldDuration);

	if (CurrentHoldTime >= SafeHoldDuration)
	{
		CompleteObjective();
	}
}

void APRHoldObjectiveActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRHoldObjectiveActor, CurrentHoldTime);
}

void APRHoldObjectiveActor::HandleObjectiveActivated(AController* ActivatingController)
{
	SetCurrentHoldTime(0.0f);
	SetObjectiveProgress(0.0f);
}

void APRHoldObjectiveActor::OnRep_CurrentHoldTime()
{
}

void APRHoldObjectiveActor::SetCurrentHoldTime(const float InCurrentHoldTime)
{
	CurrentHoldTime = FMath::Clamp(InCurrentHoldTime, 0.0f, FMath::Max(0.1f, HoldDuration));
}
