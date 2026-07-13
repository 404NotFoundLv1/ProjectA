#include "Core/PRGameState.h"

#include "Net/UnrealNetwork.h"

APRGameState::APRGameState()
{
}

void APRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APRGameState, SelectedTeamMissionId);
	DOREPLIFETIME(APRGameState, bTeamMissionReady);
}

void APRGameState::SetTeamMissionState(const FName MissionId, const bool bReady)
{
	SelectedTeamMissionId = MissionId;
	bTeamMissionReady = bReady && !MissionId.IsNone();
	ForceNetUpdate();
}
