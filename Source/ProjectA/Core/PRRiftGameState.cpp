#include "Core/PRRiftGameState.h"

#include "Net/UnrealNetwork.h"

APRRiftGameState::APRRiftGameState()
{
}

void APRRiftGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRRiftGameState, CurrentObjectiveState);
	DOREPLIFETIME(APRRiftGameState, RiftStability);
	DOREPLIFETIME(APRRiftGameState, bExtractionAvailable);
	DOREPLIFETIME(APRRiftGameState, AlivePlayerCount);
	DOREPLIFETIME(APRRiftGameState, MissionTime);
}

void APRRiftGameState::SetCurrentObjectiveState(const EPRRiftObjectiveState InCurrentObjectiveState)
{
	CurrentObjectiveState = InCurrentObjectiveState;
}

void APRRiftGameState::SetRiftStability(const float InRiftStability)
{
	RiftStability = FMath::Clamp(InRiftStability, 0.0f, 100.0f);
}

void APRRiftGameState::SetExtractionAvailable(const bool bInExtractionAvailable)
{
	bExtractionAvailable = bInExtractionAvailable;
}

void APRRiftGameState::SetAlivePlayerCount(const int32 InAlivePlayerCount)
{
	AlivePlayerCount = FMath::Max(0, InAlivePlayerCount);
}

void APRRiftGameState::SetMissionTime(const float InMissionTime)
{
	MissionTime = FMath::Max(0.0f, InMissionTime);
}

void APRRiftGameState::OnRep_CurrentObjectiveState()
{
}

void APRRiftGameState::OnRep_RiftStability()
{
}

void APRRiftGameState::OnRep_ExtractionAvailable()
{
}

void APRRiftGameState::OnRep_AlivePlayerCount()
{
}

void APRRiftGameState::OnRep_MissionTime()
{
}
