#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/PRItemTypes.h"
#include "PRInventoryWidget.generated.h"

class SScrollBox;
class STextBlock;
class UPRItemDataAsset;
class UPREquipmentComponent;
class UPRInventoryComponent;
class UPRWeaponComponent;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryItemUseRequestedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRInventoryItemDropRequestedSignature, FName, ItemId, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryItemEquipRequestedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRInventoryQuickbarAssignRequestedSignature, int32, SlotIndex, FGuid, InstanceGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryQuickbarClearRequestedSignature, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRInventoryPrimaryUnequipRequestedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryEquipmentUnequipRequestedSignature, FName, SlotId);

/**
 * Lightweight runtime inventory panel backed by the replicated owner inventory.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPRInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void BindInventory(UPRInventoryComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RefreshInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UPRInventoryComponent* GetBoundInventory() const { return BoundInventory.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	TArray<FPRItemInstance> GetDisplayedItems() const { return DisplayedItems; }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsInventoryEmpty() const { return DisplayedItems.IsEmpty(); }

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void SelectDisplayedItem(int32 ItemIndex);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool HasSelectedItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FPRItemInstance GetSelectedItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FText GetItemDisplayName(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UTexture2D* GetItemIcon(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FText GetItemRarityText(EPRItemRarity Rarity) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FLinearColor GetItemRarityColor(EPRItemRarity Rarity) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FText GetItemTooltipText(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FText GetShipResourceSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FText GetPrimaryWeaponSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FText GetEquipmentSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Quickbar")
	FText GetQuickbarSummaryText() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RequestUseSelectedItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RequestUseDisplayedItem(int32 ItemIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Quickbar", meta = (ClampMin = "0", ClampMax = "3"))
	void RequestAssignSelectedItemToQuickbar(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Quickbar", meta = (ClampMin = "0", ClampMax = "3"))
	void RequestClearQuickbarSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI", meta = (ClampMin = "1"))
	void RequestDropSelectedItem(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI", meta = (ClampMin = "1"))
	void RequestDropDisplayedItem(int32 ItemIndex, int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void RequestEquipSelectedItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void RequestUnequipPrimaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void RequestUnequipEquipmentSlot(FName SlotId);

	UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
	FPRInventoryItemUseRequestedSignature OnUseItemRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
	FPRInventoryItemDropRequestedSignature OnDropItemRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FPRInventoryItemEquipRequestedSignature OnEquipItemRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Quickbar")
	FPRInventoryQuickbarAssignRequestedSignature OnQuickbarAssignRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Quickbar")
	FPRInventoryQuickbarClearRequestedSignature OnQuickbarClearRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FPRInventoryPrimaryUnequipRequestedSignature OnUnequipPrimaryRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FPRInventoryEquipmentUnequipRequestedSignature OnUnequipEquipmentRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleWeaponStateChanged();

	UFUNCTION()
	void HandleEquipmentChanged();

	FReply HandleItemClicked(int32 ItemIndex);
	FReply HandleUseSelectedClicked();
	FReply HandleDropSelectedClicked();
	FReply HandleEquipSelectedClicked();
	FReply HandleUnequipPrimaryClicked();
	FReply HandleUnequipEquipmentSlotClicked(FName SlotId);
	void RebuildItemList();
	void RefreshSelectedItemDetails();
	void RefreshShipResourceSummary();
	void RefreshPrimaryWeaponSummary();
	void RefreshEquipmentSummary();
	UPRWeaponComponent* GetBoundWeaponComponent() const;
	UPREquipmentComponent* GetBoundEquipmentComponent() const;
	UPRItemDataAsset* FindItemData(const FPRItemInstance& Item) const;
	FString BuildItemSummary(const FPRItemInstance& Item) const;
	FText BuildSelectedItemDetails() const;

	TWeakObjectPtr<UPRInventoryComponent> BoundInventory;

	UPROPERTY(Transient)
	TArray<FPRItemInstance> DisplayedItems;

	int32 SelectedItemIndex = INDEX_NONE;

	TSharedPtr<SScrollBox> ItemScrollBox;
	TSharedPtr<STextBlock> DetailTextBlock;
	TSharedPtr<STextBlock> ShipResourceSummaryTextBlock;
	TSharedPtr<STextBlock> PrimaryWeaponSummaryTextBlock;
	TSharedPtr<STextBlock> EquipmentSummaryTextBlock;
};
