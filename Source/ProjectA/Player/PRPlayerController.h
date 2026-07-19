#pragma once

#include "Abilities/PRCombatFeedbackTypes.h"
#include "CoreMinimal.h"
#include "Items/PRItemTypes.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Roles/PRRoleTypes.h"
#include "Ship/PRShipRepairTypes.h"
#include "ProjectAPlayerController.h"
#include "TimerManager.h"
#include "PRPlayerController.generated.h"

class UPRGASDebugWidget;
class UPRInventoryComponent;
class UPRInventoryWidget;
class UPRLobbyReadyDebugWidget;
class UPRRiftSettlementWidget;
class UPRShipRepairWidget;
class UPRRoleLoadoutWidget;
class UPRDiagnosticsWidget;
class UPRWeaponHUDWidget;
class UPRLootTableDataAsset;
class UGameplayEffect;
class APRPickupActor;
class APRRiftObjectiveActor;
class APRCharacter;

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

	UFUNCTION(BlueprintCallable, Category = "Revive")
	void TryBeginRevive(APRCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable, Category = "Revive")
	void CancelActiveRevive();

	UFUNCTION(BlueprintCallable, Category = "Rift|Objective")
	void TryActivateRiftObjective();

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

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void EquipInventoryWeapon(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void UnequipPrimaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void UnequipEquipmentSlot(FName SlotId);

	UFUNCTION(BlueprintCallable, Category = "Loot|Debug")
	void SpawnTestLoot();

	/** Development-only grant for validating the v0.7.1 non-weapon equipment flow. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
	void GiveTestEquipmentKit();

	UFUNCTION(BlueprintCallable, Category = "Diagnostics|UI")
	void ToggleDiagnosticsPanel();

	UFUNCTION(BlueprintCallable, Category = "Diagnostics|UI")
	void HideDiagnosticsPanel();

	UFUNCTION(BlueprintPure, Category = "Diagnostics|UI")
	bool IsDiagnosticsPanelVisible() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsInventoryVisible() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UPRInventoryWidget* GetInventoryWidget() const { return InventoryWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Weapon|UI")
	UPRWeaponHUDWidget* GetWeaponHUDWidget() const { return WeaponHUDWidget.Get(); }

	/** Server-only endpoint used by resolved combat feedback to notify this owning client. */
	void SendHitConfirmationToOwner(const FPRHitConfirmation& Confirmation);

	UFUNCTION(Client, Unreliable, Category = "Combat|Feedback")
	void ClientReceiveHitConfirmation(const FPRHitConfirmation& Confirmation);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	FPRHitConfirmation LastHitConfirmation;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 HitConfirmationSentCount = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Feedback")
	int32 HitConfirmationReceivedCount = 0;

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void ToggleShipRepairPanel();

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void ShowShipRepairPanel();

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void HideShipRepairPanel();

	UFUNCTION(BlueprintPure, Category = "Ship Repair|UI")
	bool IsShipRepairPanelVisible() const;

	UFUNCTION(BlueprintPure, Category = "Ship Repair|UI")
	UPRShipRepairWidget* GetShipRepairWidget() const { return ShipRepairWidget.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void ToggleRoleLoadoutPanel();

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void ShowRoleLoadoutPanel();

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void HideRoleLoadoutPanel();

	UFUNCTION(BlueprintPure, Category = "Roles|UI")
	UPRRoleLoadoutWidget* GetRoleLoadoutWidget() const { return RoleLoadoutWidget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	UPRRiftSettlementWidget* GetRiftSettlementWidget() const { return RiftSettlementWidget.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	APRPickupActor* FindBestPickupCandidate() const;

	UFUNCTION(BlueprintCallable, Category = "Revive")
	APRCharacter* FindBestReviveCandidate() const;

	UFUNCTION(BlueprintCallable, Category = "Rift|Objective")
	APRRiftObjectiveActor* FindBestRiftObjectiveCandidate() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* FindFocusedInteractionTarget() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsFocusedInteractionTarget(const AActor* TargetActor) const;

	bool TryPickupOnServer(APRPickupActor* PickupActor);
	bool TryActivateRiftObjectiveOnServer(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
	void ServerSetReady(bool bReady);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Role")
	void ServerSetSelectedRoleModule(FName RoleModule);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Role")
	void ServerApplyRoleLoadout(FName RoleId, const FPRRoleLoadout& Loadout);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Deployable")
	void ServerConfirmDeployable(const FTransform& ClientTransform, int32 SessionSequence);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Deployable")
	void ServerCancelDeployable();

	UFUNCTION(Client, Reliable, Category = "Lobby|Role")
	void ClientRoleLoadoutApplyResult(EPRRoleLoadoutApplyResult Result, const FString& Diagnostic);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby|Travel")
	void ServerStartRiftMission();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Pickup")
	void ServerTryPickup(APRPickupActor* PickupActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Revive")
	void ServerBeginRevive(APRCharacter* TargetCharacter);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Revive")
	void ServerCancelRevive();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rift|Objective")
	void ServerTryActivateRiftObjective(APRRiftObjectiveActor* ObjectiveActor);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Use")
	void ServerUseInventoryItem(FName ItemId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Drop")
	void ServerDropInventoryItem(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Equipment")
	void ServerEquipInventoryWeapon(FName ItemId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Equipment")
	void ServerUnequipPrimaryWeapon();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory|Equipment")
	void ServerUnequipEquipmentSlot(FName SlotId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Loot|Debug")
	void ServerSpawnTestLoot();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment|Debug")
	void ServerGiveTestEquipmentKit();

	UFUNCTION(BlueprintCallable, Category = "Lobby|Profile")
	void SubmitLocalMultiplayerProfile();

	UFUNCTION(Server, Reliable, Category = "Lobby|Profile")
	void ServerBindMultiplayerProfile(const FPRMultiplayerProfileProjection& Projection);

	UFUNCTION(Server, Reliable, Category = "Lobby|Profile")
	void ServerReportPendingSettlementSave();

	UFUNCTION(Client, Reliable, Category = "Lobby|Profile")
	void ClientMultiplayerProfileBindingResult(bool bAccepted, const FString& Diagnostic);

	UFUNCTION(Client, Reliable, Category = "Rift|Settlement")
	void ClientReceivePersonalSettlement(const FPRPlayerSettlementReceipt& Receipt);

	UFUNCTION(Server, Reliable, Category = "Rift|Settlement")
	void ServerAcknowledgePersonalSettlement(FGuid SettlementId, bool bSaveSucceeded);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Ship Repair")
	void RequestShipRepair(FName RepairProjectId);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Ship Repair")
	void RetryPendingShipRepairSave();

	UFUNCTION(Server, Reliable, Category = "Lobby|Ship Repair")
	void ServerRequestShipRepair(FName RepairProjectId);

	UFUNCTION(Client, Reliable, Category = "Lobby|Ship Repair")
	void ClientReceiveShipRepairReceipt(const FPRShipRepairReceipt& Receipt);

	UFUNCTION(Server, Reliable, Category = "Lobby|Ship Repair")
	void ServerAcknowledgeShipRepair(FGuid TransactionId, bool bSaveSucceeded);

	UFUNCTION(Server, Reliable, Category = "Lobby|Ship Repair")
	void ServerReportPendingShipRepairSave(FGuid TransactionId);

	UFUNCTION(Client, Reliable, Category = "Lobby|Ship Repair")
	void ClientShipRepairFailed(FName RepairProjectId, const FString& Diagnostic);

	UFUNCTION(BlueprintPure, Category = "Lobby|Profile")
	FString GetMultiplayerProfileStatus() const { return MultiplayerProfileStatus; }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	FString GetPersonalSettlementSaveStatus() const { return PersonalSettlementSaveStatus; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Ship Repair")
	FString GetShipRepairSaveStatus() const { return ShipRepairSaveStatus; }

	UFUNCTION(BlueprintCallable, Category = "Lobby|Debug")
	void RefreshLobbyReadyDebugDisplay();

	bool TryUseInventoryItemOnServer(FName ItemId);
	bool TryDropInventoryItemOnServer(FName ItemId, int32 Count);
	APRPickupActor* TrySpawnTestLootOnServer(float RollOverride = -1.0f);
	bool TryGiveTestEquipmentKitOnServer();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnRep_PlayerState() override;

private:
	void CreateGASDebugHUD();
	void DestroyGASDebugHUD();
	void CreateLobbyReadyDebugHUD();
	void DestroyLobbyReadyDebugHUD();
	void ShowDiagnosticsPanel(bool bOpenToolsTab = false);
	void CreateDiagnosticsUI();
	void DestroyDiagnosticsUI();
	void ApplyDiagnosticsInputMode();
	void RestoreDiagnosticsInputMode();
	void CreateInventoryUI();
	void DestroyInventoryUI();
	void CreateWeaponHUD();
	void DestroyWeaponHUD();
	void CreateRiftSettlementUI();
	void DestroyRiftSettlementUI();
	void CreateShipRepairUI();
	void DestroyShipRepairUI();
	void CreateRoleLoadoutUI();
	void DestroyRoleLoadoutUI();
	void ApplyInventoryInputMode();
	void RestoreInventoryInputMode();
	void ApplyShipRepairInputMode();
	void RestoreShipRepairInputMode();
	void ApplyRoleLoadoutInputMode();
	void RestoreRoleLoadoutInputMode();
	void CancelPendingDeployable();
	UPRInventoryComponent* GetLocalInventoryComponent() const;
	TSubclassOf<UGameplayEffect> ResolveConsumableEffectClass(FName ItemId) const;
	bool CanUseInventoryItemOnServer(FName ItemId, FString* OutFailureReason = nullptr) const;
	bool CanDropInventoryItemOnServer(FName ItemId, int32 Count, FString* OutFailureReason = nullptr) const;
	APRPickupActor* SpawnDroppedPickupOnServer(const FPRItemInstance& DroppedItem) const;

	UFUNCTION()
	void HandleInventoryUseRequested(FName ItemId);

	UFUNCTION()
	void HandleInventoryDropRequested(FName ItemId, int32 Count);

	UFUNCTION()
	void HandleInventoryEquipRequested(FName ItemId);

	UFUNCTION()
	void HandlePrimaryWeaponUnequipRequested();

	UFUNCTION()
	void HandleEquipmentUnequipRequested(FName SlotId);

	bool CanServerPickup(APRPickupActor* PickupActor, FString* OutFailureReason = nullptr) const;
	bool TryBeginReviveOnServer(APRCharacter* TargetCharacter);
	bool CanServerActivateRiftObjective(APRRiftObjectiveActor* ObjectiveActor, FString* OutFailureReason = nullptr) const;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "0.0"))
	float PickupInteractionRadius = 350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rift|Objective", meta = (ClampMin = "0.0"))
	float ObjectiveInteractionRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GAS|Debug")
	TSubclassOf<UPRGASDebugWidget> GASDebugWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRGASDebugWidget> GASDebugWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPRLobbyReadyDebugWidget> LobbyReadyDebugWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Diagnostics|UI")
	TSubclassOf<UPRDiagnosticsWidget> DiagnosticsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRDiagnosticsWidget> DiagnosticsWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI")
	TSubclassOf<UPRInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRInventoryWidget> InventoryWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|UI")
	TSubclassOf<UPRWeaponHUDWidget> WeaponHUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRWeaponHUDWidget> WeaponHUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Rift|Settlement")
	TSubclassOf<UPRRiftSettlementWidget> RiftSettlementWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRRiftSettlementWidget> RiftSettlementWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Ship Repair|UI")
	TSubclassOf<UPRShipRepairWidget> ShipRepairWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRShipRepairWidget> ShipRepairWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Roles|UI")
	TSubclassOf<UPRRoleLoadoutWidget> RoleLoadoutWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPRRoleLoadoutWidget> RoleLoadoutWidget;

	bool bInventoryInputModeActive = false;
	bool bSavedMouseCursorVisibilityForInventory = false;
	bool bShipRepairInputModeActive = false;
	bool bSavedMouseCursorVisibilityForShipRepair = false;
	bool bRoleLoadoutInputModeActive = false;
	bool bSavedMouseCursorVisibilityForRoleLoadout = false;
	bool bDiagnosticsInputModeActive = false;

	TWeakObjectPtr<APRCharacter> ActiveReviveTarget;
	bool bSavedMouseCursorVisibilityForDiagnostics = false;

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
	FTimerHandle ShipRepairRetryTimerHandle;
	FString MultiplayerProfileStatus = TEXT("Not bound");
	FString PersonalSettlementSaveStatus = TEXT("Waiting");
	FString ShipRepairSaveStatus = TEXT("Idle");
};
