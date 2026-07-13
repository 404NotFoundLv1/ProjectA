#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
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

	void SetTeamMissionState(FName MissionId, bool bReady);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Mission", meta = (AllowPrivateAccess = "true"))
	FName SelectedTeamMissionId = NAME_None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Mission", meta = (AllowPrivateAccess = "true"))
	bool bTeamMissionReady = false;
};
