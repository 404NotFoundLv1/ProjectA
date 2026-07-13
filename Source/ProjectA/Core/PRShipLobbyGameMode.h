#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameModeBase.h"
#include "PRShipLobbyGameMode.generated.h"

class APlayerController;
class APlayerState;
class UPRMissionProgressionDataAsset;

/**
 * Listen-server lobby rules for starting the first rift test map.
 */
UCLASS()
class PROJECTA_API APRShipLobbyGameMode : public APRGameModeBase
{
	GENERATED_BODY()

public:
	APRShipLobbyGameMode();

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	FString GetRiftTestMapPath() const { return RiftTestMapPath; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	FString BuildRiftTravelURL() const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	bool ArePlayerStatesReadyForTravel(const TArray<APlayerState*>& PlayerStates) const;

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	bool CanStartRiftMission() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Travel")
	bool StartRiftMission(APlayerController* RequestingController);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Mission")
	void RefreshTeamMissionState();

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	FString RiftTestMapPath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Mission")
	FName DefaultMissionId = FName(TEXT("Mission.Rift.Test.Hold"));

private:
	bool IsHostPlayerController(const APlayerController* RequestingController) const;
	UPRMissionProgressionDataAsset* ResolveSelectedMission() const;
	APlayerController* FindHostPlayerController() const;
};
