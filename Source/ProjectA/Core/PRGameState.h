#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Progression/PRMissionContractTypes.h"
#include "PRGameState.generated.h"

/**
 * Replicated global state shared by lobby and rift flows.
 */
UCLASS()
class PROJECTA_API APRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APRGameState();

	UFUNCTION(BlueprintPure, Category = "Lobby|Mission")
	FName GetSelectedTeamMissionId() const { return SelectedTeamMissionId; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Mission")
	bool IsTeamMissionReady() const { return bTeamMissionReady; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Mission")
	FPRMissionDefinition GetSelectedTeamMissionDefinition() const { return SelectedTeamMissionDefinition; }

	void SetTeamMissionState(FName MissionId, bool bReady);
	void SetTeamMissionDefinition(const FPRMissionDefinition& MissionDefinition, bool bReady);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Mission", meta = (AllowPrivateAccess = "true"))
	FName SelectedTeamMissionId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Mission", meta = (AllowPrivateAccess = "true"))
	FPRMissionDefinition SelectedTeamMissionDefinition;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Mission", meta = (AllowPrivateAccess = "true"))
	bool bTeamMissionReady = false;
};
