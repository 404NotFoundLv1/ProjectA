#pragma once

#include "CoreMinimal.h"
#include "ProjectAPlayerController.h"
#include "TimerManager.h"
#include "PRPlayerController.generated.h"

class UPRGASDebugWidget;
class UPRInventoryComponent;
class UPRInventoryWidget;
class UGameplayEffect;
class APRPickupActor;

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

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	void TryPickup();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void ShowInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void HideInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RefreshInventoryDisplay();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Use")
	void UseInventoryItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsInventoryVisible() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UPRInventoryWidget* GetInventoryWidget() const { return InventoryWidget.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	APRPickupActor* FindBestPickupCandidate() const;

	bool TryPickupOnServer(APRPickupActor* PickupActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Role")
	void ServerSetSelectedRoleModule(FName RoleModule);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Travel")
	void ServerStartRiftMission();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Pickup")
	void ServerTryPickup(APRPickupActor* PickupActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Use")
	void ServerUseInventoryItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Debug")
	void RefreshLobbyReadyDebugDisplay();

	bool TryUseInventoryItemOnServer(FName ItemId);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;

private:
	void CreateGASDebugHUD();
	void DestroyGASDebugHUD();
	void CreateInventoryUI();
	void DestroyInventoryUI();
	UPRInventoryComponent* GetLocalInventoryComponent() const;
	TSubclassOf<UGameplayEffect> ResolveConsumableEffectClass(FName ItemId) const;
	bool CanUseInventoryItemOnServer(FName ItemId, FString* OutFailureReason = nullptr) const;

	UFUNCTION()
	void HandleInventoryUseRequested(FName ItemId);

	bool CanServerPickup(APRPickupActor* PickupActor, FString* OutFailureReason = nullptr) const;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "0.0"))
	float PickupInteractionRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Debug")
	TSubclassOf<UPRGASDebugWidget> GASDebugWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRGASDebugWidget> GASDebugWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UPRInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRInventoryWidget> InventoryWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Use")
	TSubclassOf<UGameplayEffect> HealthInjectorEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Use")
	TSubclassOf<UGameplayEffect> ShieldPackEffectClass;

	FTimerHandle LobbyReadyDebugTimerHandle;
};
