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
	DOREPLIFETIME(APRRiftGameState, ExtractedPlayerCount);
	DOREPLIFETIME(APRRiftGameState, bExtractionComplete);
	DOREPLIFETIME(APRRiftGameState, AlivePlayerCount);
	DOREPLIFETIME(APRRiftGameState, MissionTime);
	DOREPLIFETIME(APRRiftGameState, ObjectiveProgress);
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

void APRRiftGameState::SetExtractedPlayerCount(const int32 InExtractedPlayerCount)
{
	ExtractedPlayerCount = FMath::Max(0, InExtractedPlayerCount);
}

void APRRiftGameState::SetExtractionComplete(const bool bInExtractionComplete)
{
	bExtractionComplete = bInExtractionComplete;
}

void APRRiftGameState::SetAlivePlayerCount(const int32 InAlivePlayerCount)
{
	AlivePlayerCount = FMath::Max(0, InAlivePlayerCount);
}

void APRRiftGameState::SetMissionTime(const float InMissionTime)
{
	MissionTime = FMath::Max(0.0f, InMissionTime);
}

void APRRiftGameState::SetObjectiveProgress(const float InObjectiveProgress)
{
	ObjectiveProgress = FMath::Clamp(InObjectiveProgress, 0.0f, 1.0f);
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

void APRRiftGameState::OnRep_ExtractedPlayerCount()
{
}

void APRRiftGameState::OnRep_ExtractionComplete()
{
}

void APRRiftGameState::OnRep_AlivePlayerCount()
{
}

void APRRiftGameState::OnRep_MissionTime()
{
}

void APRRiftGameState::OnRep_ObjectiveProgress()
{
}
