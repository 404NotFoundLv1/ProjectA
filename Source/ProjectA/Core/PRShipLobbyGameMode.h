#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameModeBase.h"
#include "PRShipLobbyGameMode.generated.h"

class APlayerController;
class APlayerState;

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

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	FString RiftTestMapPath;

private:
	bool IsHostPlayerController(const APlayerController* RequestingController) const;
};
