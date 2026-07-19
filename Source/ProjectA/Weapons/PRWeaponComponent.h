#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Persistence/PRProfileTypes.h"
#include "Weapons/PRWeaponTypes.h"
#include "PRWeaponComponent.generated.h"

class APRCharacter;
class APRWeaponActor;
class UPRWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRWeaponStateChangedDelegate);

/** Server-authoritative weapon/equipment state that survives pawn replacement on PlayerState. */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRWeaponComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FPRWeaponStateChangedDelegate OnWeaponStateChanged;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FPRItemInstance GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UPRWeaponDataAsset* GetEquippedWeaponData() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserveAmmo() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bAiming; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bReloading; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	bool EquipWeaponFromInventory(FName ItemId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	bool UnequipWeapon();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	EPRWeaponFireResult TryFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool StartReload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CancelReload();

	bool ReplaceEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic);
	TArray<FPRProfileEquipmentEntry> GetEquipmentEntries() const;
	bool BuildPersistentBackpack(TArray<FPRItemInstance>& OutBackpack, FString& OutDiagnostic) const;
	bool EnsureStarterWeapon(FName StarterWeaponItemId, FString& OutDiagnostic);
	void CopyRuntimeStateFrom(const UPRWeaponComponent* SourceComponent);
	void HandleAvatarChanged(APRCharacter* NewAvatar);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Client, Reliable)
	void ClientAimingResult(bool bAccepted, bool bAuthoritativeAiming);

	UFUNCTION(Server, Reliable)
	void ServerStartReload();

	UFUNCTION(Server, Reliable)
	void ServerCancelReload();

	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION()
	void OnRep_MagazineAmmo();

	UFUNCTION()
	void OnRep_Aiming();

	UFUNCTION()
	void OnRep_Reloading();

	bool CanUseWeapon() const;
	bool SetAimingAuthoritative(bool bNewAiming);
	bool BeginReloadAuthoritative();
	void FinishReload();
	void CancelReloadAuthoritative();
	void SetReloadingAuthoritative(bool bNewReloading);
	void SetStateTag(FGameplayTag Tag, bool bEnabled);
	void RefreshWeaponActor();
	void DestroyWeaponActor();
	void NotifyStateChanged();
	int32 FindPrimaryEquipmentIndex() const;
	bool SetPrimaryEquipment(const FPRItemInstance& Item);
	bool ClearPrimaryEquipment();
	class UPREquipmentComponent* GetEquipmentComponent() const;
	bool AddMagazineToInventory(const UPRWeaponDataAsset* WeaponData);
	bool LoadMagazineFromInventory(const UPRWeaponDataAsset* WeaponData);
	bool RestoreTransaction(
		const TArray<FPRItemInstance>& InventoryItems,
		const TArray<FPRProfileEquipmentEntry>& Equipment,
		const FPRItemInstance& Weapon,
		int32 Ammo);

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, VisibleInstanceOnly, Category = "Weapon")
	FPRItemInstance EquippedWeapon;

	UPROPERTY(ReplicatedUsing = OnRep_MagazineAmmo, VisibleInstanceOnly, Category = "Weapon")
	int32 MagazineAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Aiming, VisibleInstanceOnly, Category = "Weapon")
	bool bAiming = false;

	UPROPERTY(ReplicatedUsing = OnRep_Reloading, VisibleInstanceOnly, Category = "Weapon")
	bool bReloading = false;

	UPROPERTY(Transient)
	TObjectPtr<APRCharacter> CurrentAvatar;

	UPROPERTY(Transient)
	TObjectPtr<APRWeaponActor> SpawnedWeaponActor;

	FTimerHandle ReloadTimerHandle;
	FGuid ActiveReloadTransactionId;
	double LastFireTime = -TNumericLimits<double>::Max();
};
