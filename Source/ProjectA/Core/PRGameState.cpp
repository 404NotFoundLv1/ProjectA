#include "Core/PRGameState.h"

#include "Net/UnrealNetwork.h"

APRGameState::APRGameState()
{
}

void APRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APRGameState, SelectedTeamMissionId);
	DOREPLIFETIME(APRGameState, SelectedTeamMissionDefinition);
	DOREPLIFETIME(APRGameState, bTeamMissionReady);
}

void APRGameState::SetTeamMissionState(const FName MissionId, const bool bReady)
{
	SelectedTeamMissionId = MissionId;
	SelectedTeamMissionDefinition = FPRMissionDefinition();
	bTeamMissionReady = bReady && !MissionId.IsNone();
	ForceNetUpdate();
}

void APRGameState::SetTeamMissionDefinition(const FPRMissionDefinition& MissionDefinition, const bool bReady)
{
	SelectedTeamMissionDefinition = MissionDefinition;
	SelectedTeamMissionId = MissionDefinition.ContractId;
	bTeamMissionReady = bReady && MissionDefinition.IsValid();
	ForceNetUpdate();
}
