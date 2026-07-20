#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/PRQuickbarTypes.h"
#include "PRQuickbarComponent.generated.h"

class UPRAbilitySystemComponent;
class UPRInventoryComponent;
class UPRItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRQuickbarChangedDelegate);

/** PlayerState-owned, owner-only replicated references to up to four inventory item instances. */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRQuickbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRQuickbarComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Quickbar")
	FPRQuickbarChangedDelegate OnQuickbarChanged;

	UFUNCTION(BlueprintPure, Category = "Quickbar")
	TArray<FPRQuickSlotReference> GetQuickSlots() const { return QuickSlots; }

	UFUNCTION(BlueprintPure, Category = "Quickbar")
	FPRQuickbarUseState GetActiveUse() const { return ActiveUse; }

	UFUNCTION(BlueprintPure, Category = "Quickbar")
	TArray<FPRQuickbarCooldownState> GetCooldowns() const { return Cooldowns; }

	UFUNCTION(BlueprintPure, Category = "Quickbar")
	bool IsUsingItem() const { return ActiveUse.IsActive(); }

	UFUNCTION(BlueprintCallable, Category = "Quickbar")
	bool AssignSlot(int32 SlotIndex, FGuid InstanceGuid);

	UFUNCTION(BlueprintCallable, Category = "Quickbar")
	bool ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Quickbar")
	bool RequestUseSlot(int32 SlotIndex);

	/** Starts the same authoritative, timed use transaction for an inventory item outside a bound quick slot. */
	bool RequestUseInstance(FGuid InstanceGuid);

	UFUNCTION(BlueprintCallable, Category = "Quickbar")
	void CancelActiveUse();

	void CopyRuntimeStateFrom(const UPRQuickbarComponent* SourceComponent);
	void ResetForNewProfileBinding();
	void SetQuickSlotsFromProfile(const TArray<FPRQuickSlotReference>& InQuickSlots);
	void ReconcileInventoryReferences();

	UFUNCTION(Server, Reliable, Category = "Quickbar")
	void ServerAssignSlot(int32 SlotIndex, FGuid InstanceGuid);

	UFUNCTION(Server, Reliable, Category = "Quickbar")
	void ServerClearSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable, Category = "Quickbar")
	void ServerRequestUseSlot(int32 SlotIndex);

	UFUNCTION(Server, Reliable, Category = "Quickbar")
	void ServerRequestUseInstance(FGuid InstanceGuid);

	UFUNCTION(Server, Reliable, Category = "Quickbar")
	void ServerCancelActiveUse();

private:
	static constexpr int32 SlotCount = 4;

	UPRInventoryComponent* GetInventory() const;
	UPRAbilitySystemComponent* GetAbilitySystem() const;
	const FPRQuickSlotReference* FindSlot(int32 SlotIndex) const;
	UPRItemDataAsset* FindItemDefinition(const FGuid& InstanceGuid) const;
	bool IsSlotIndexValid(int32 SlotIndex) const;
	bool IsCooldownActive(const FGameplayTag& CooldownTag) const;
	bool ValidateUse(const FPRQuickSlotReference& Slot, UPRItemDataAsset*& OutDefinition, FString& OutReason, bool bAllowActiveUse = false) const;
	bool BeginUseAuthoritative(const FPRQuickSlotReference& SourceReference);
	void CompleteActiveUse();
	void CancelActiveUseAuthoritative(const FString& Reason);
	void SetUsingTag(bool bEnabled);
	void StartCooldown(const FPRItemUseDefinition& Definition);
	void BroadcastChanged();

	UPROPERTY(ReplicatedUsing = OnRep_QuickSlots, VisibleInstanceOnly, BlueprintReadOnly, Category = "Quickbar", meta = (AllowPrivateAccess = "true"))
	TArray<FPRQuickSlotReference> QuickSlots;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveUse, VisibleInstanceOnly, BlueprintReadOnly, Category = "Quickbar", meta = (AllowPrivateAccess = "true"))
	FPRQuickbarUseState ActiveUse;

	UPROPERTY(ReplicatedUsing = OnRep_Cooldowns, VisibleInstanceOnly, BlueprintReadOnly, Category = "Quickbar", meta = (AllowPrivateAccess = "true"))
	TArray<FPRQuickbarCooldownState> Cooldowns;

	FTimerHandle ActiveUseTimerHandle;
	UFUNCTION()
	void OnRep_QuickSlots();

	UFUNCTION()
	void OnRep_ActiveUse();

	UFUNCTION()
	void OnRep_Cooldowns();

	UFUNCTION()
	void HandleInventoryChanged();
};
