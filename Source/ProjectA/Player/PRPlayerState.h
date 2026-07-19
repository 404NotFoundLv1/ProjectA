#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Ship/PRShipRepairTypes.h"
#include "PRPlayerState.generated.h"

class UPRAbilitySystemComponent;
class UPRAttributeSet;
class UAbilitySystemComponent;
class UPRInventoryComponent;
class UPRItemTransactionComponent;
class UPRMissionProgressionDataAsset;
class UPRWeaponComponent;
class UPRRoleComponent;
class UPRDeployableComponent;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRShipResourceStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship|Resources")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ship|Resources", meta = (ClampMin = "0"))
	int32 Count = 0;

	bool IsValid() const
	{
		return !ResourceId.IsNone() && Count > 0;
	}
};

/**
 * Player-owned replicated data that should survive pawn replacement.
 */
UCLASS()
class PROJECTA_API APRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APRPlayerState();

	virtual void BeginPlay() override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAttributeSet* GetAttributeSet() const { return AttributeSet.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPRInventoryComponent* GetInventoryComponent() const { return InventoryComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Item Transaction")
	UPRItemTransactionComponent* GetItemTransactionComponent() const { return ItemTransactionComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UPRWeaponComponent* GetWeaponComponent() const { return WeaponComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Roles")
	UPRRoleComponent* GetRoleComponent() const { return RoleComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Deployable")
	UPRDeployableComponent* GetDeployableComponent() const { return DeployableComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Profile")
	bool IsMultiplayerProfileBound() const { return bMultiplayerProfileBound; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Profile")
	FGuid GetBoundProfileId() const { return BoundProfileId; }

	const FPRProfileStoryProgress& GetBoundStoryProgress() const { return BoundStoryProgress; }
	const TArray<FPRProfileShipModuleState>& GetBoundShipModules() const { return BoundShipModules; }
	const TArray<FPRItemInstance>& GetMissionStartBackpackItems() const { return MissionStartBackpackItems; }
	const TArray<FPRProfileResourceBalance>& GetMissionStartResourceWallet() const { return MissionStartResourceWallet; }
	FName GetMissionStartRoleId() const { return MissionStartRoleId; }
	const TArray<FName>& GetMissionStartUnlockedRoleIds() const { return MissionStartUnlockedRoleIds; }
	const TArray<FName>& GetMissionStartUnlockedRoleModuleIds() const { return MissionStartUnlockedRoleModuleIds; }
	const FPRRoleLoadout& GetMissionStartRoleLoadout() const { return MissionStartRoleLoadout; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Ship Repair")
	bool IsRepairPersistencePending() const { return bRepairPersistencePending; }

	UFUNCTION(BlueprintPure, Category = "Lobby|Ship Repair")
	FGuid GetPendingRepairTransactionId() const { return PendingRepairTransactionId; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Profile")
	bool BindMultiplayerProfile(const FPRMultiplayerProfileProjection& Projection, FString& OutDiagnostic);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Profile")
	void ClearMultiplayerProfileBinding();

	bool ApplyMissionCompletion(const UPRMissionProgressionDataAsset* Mission);
	bool BuildBoundShipRepairSnapshot(FPRProfileSnapshot& OutSnapshot, FString& OutDiagnostic) const;
	bool ApplyShipRepairState(const FPRShipRepairReceipt& Receipt, FString& OutDiagnostic);
	void SetRepairPersistencePending(FGuid TransactionId, bool bPending);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetReady(bool bReady);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FName GetSelectedRoleModule() const { return SelectedRoleModule; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetSelectedRoleModule(FName InSelectedRoleModule);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FString GetPlayerDisplayName() const { return PlayerDisplayName; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetPlayerDisplayName(const FString& InPlayerDisplayName);

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	TArray<FPRShipResourceStack> GetShipResources() const { return ShipResources; }

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetShipResourceCount(FName ResourceId) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ship|Resources")
	bool AddShipResource(FName ResourceId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ship|Resources")
	void ResetShipResources();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ship|Resources")
	bool ReplaceShipResources(const TArray<FPRShipResourceStack>& Resources);

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	FString BuildShipResourceSummary() const;

private:
	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_PlayerDisplayName();

	UFUNCTION()
	void OnRep_ShipResources();

	int32 FindShipResourceIndex(FName ResourceId) const;
	void CopyProjectRiftStateFrom(const APRPlayerState* SourcePlayerState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Transaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRItemTransactionComponent> ItemTransactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roles", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRRoleComponent> RoleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deployable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRDeployableComponent> DeployableComponent;

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsReady;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Profile", meta = (AllowPrivateAccess = "true"))
	bool bMultiplayerProfileBound = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Profile", meta = (AllowPrivateAccess = "true"))
	FGuid BoundProfileId;

	UPROPERTY(Replicated)
	FPRProfileStoryProgress BoundStoryProgress;

	UPROPERTY(Replicated)
	TArray<FPRProfileShipModuleState> BoundShipModules;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Ship Repair", meta = (AllowPrivateAccess = "true"))
	bool bRepairPersistencePending = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby|Ship Repair", meta = (AllowPrivateAccess = "true"))
	FGuid PendingRepairTransactionId;

	UPROPERTY()
	TArray<FPRItemInstance> MissionStartBackpackItems;

	UPROPERTY()
	TArray<FPRProfileResourceBalance> MissionStartResourceWallet;

	UPROPERTY()
	FName MissionStartRoleId = NAME_None;

	UPROPERTY()
	TArray<FName> MissionStartUnlockedRoleIds;

	UPROPERTY()
	TArray<FName> MissionStartUnlockedRoleModuleIds;

	UPROPERTY()
	FPRRoleLoadout MissionStartRoleLoadout;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FName SelectedRoleModule;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerDisplayName, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FString PlayerDisplayName;

	UPROPERTY(ReplicatedUsing = OnRep_ShipResources, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TArray<FPRShipResourceStack> ShipResources;
};
