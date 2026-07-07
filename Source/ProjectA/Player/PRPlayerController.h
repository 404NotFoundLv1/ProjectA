#pragma once

#include "CoreMinimal.h"
#include "Items/PRItemTypes.h"
#include "ProjectAPlayerController.h"
#include "TimerManager.h"
#include "PRPlayerController.generated.h"

class UPRGASDebugWidget;
class UPRInventoryComponent;
class UPRInventoryWidget;
class UPRLootTableDataAsset;
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

	UFUNCTION(BlueprintCallable, Category = "Inventory|Drop", meta = (ClampMin = "1"))
	void DropInventoryItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Loot|Debug")
	void SpawnTestLoot();

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

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Drop")
	void ServerDropInventoryItem(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loot|Debug")
	void ServerSpawnTestLoot();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Debug")
	void RefreshLobbyReadyDebugDisplay();

	bool TryUseInventoryItemOnServer(FName ItemId);
	bool TryDropInventoryItemOnServer(FName ItemId, int32 Count);
	APRPickupActor* TrySpawnTestLootOnServer(float RollOverride = -1.0f);

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
	void ApplyInventoryInputMode();
	void RestoreInventoryInputMode();
	UPRInventoryComponent* GetLocalInventoryComponent() const;
	TSubclassOf<UGameplayEffect> ResolveConsumableEffectClass(FName ItemId) const;
	bool CanUseInventoryItemOnServer(FName ItemId, FString* OutFailureReason = nullptr) const;
	bool CanDropInventoryItemOnServer(FName ItemId, int32 Count, FString* OutFailureReason = nullptr) const;
	APRPickupActor* SpawnDroppedPickupOnServer(const FPRItemInstance& DroppedItem) const;

	UFUNCTION()
	void HandleInventoryUseRequested(FName ItemId);

	UFUNCTION()
	void HandleInventoryDropRequested(FName ItemId, int32 Count);

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

	bool bInventoryInputModeActive = false;
	bool bSavedMouseCursorVisibilityForInventory = false;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Use")
	TSubclassOf<UGameplayEffect> HealthInjectorEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Use")
	TSubclassOf<UGameplayEffect> ShieldPackEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop")
	TSubclassOf<APRPickupActor> PickupActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop", meta = (ClampMin = "0.0"))
	float DropForwardDistance = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Drop")
	float DropHeightOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Loot|Debug")
	TObjectPtr<UPRLootTableDataAsset> TestLootTable;

	FTimerHandle LobbyReadyDebugTimerHandle;
};
