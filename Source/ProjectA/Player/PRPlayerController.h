#pragma once

#include "CoreMinimal.h"
#include "ProjectAPlayerController.h"
#include "TimerManager.h"
#include "PRPlayerController.generated.h"

/**
 * Player-owned input and UI entry point for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRPlayerController : public AProjectAPlayerController
{
	GENERATED_BODY()

public:
	APRPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void ToggleReady();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
	void ServerSetReady(bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Debug")
	void RefreshLobbyReadyDebugDisplay();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	FTimerHandle LobbyReadyDebugTimerHandle;
};
