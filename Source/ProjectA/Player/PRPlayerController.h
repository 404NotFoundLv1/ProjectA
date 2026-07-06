#pragma once

#include "CoreMinimal.h"
#include "ProjectAPlayerController.h"
#include "TimerManager.h"
#include "PRPlayerController.generated.h"

class UPRGASDebugWidget;

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

	UFUNCTION(BlueprintCallable, Category = "Lobby|Role")
	void SelectAssaultRoleModule();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Role")
	void SelectRoleModule(FName RoleModule);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Travel")
	void StartRiftMission();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Role")
	void ServerSetSelectedRoleModule(FName RoleModule);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Travel")
	void ServerStartRiftMission();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Debug")
	void RefreshLobbyReadyDebugDisplay();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void CreateGASDebugHUD();
	void DestroyGASDebugHUD();

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Debug")
	TSubclassOf<UPRGASDebugWidget> GASDebugWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRGASDebugWidget> GASDebugWidget;

	FTimerHandle LobbyReadyDebugTimerHandle;
};
