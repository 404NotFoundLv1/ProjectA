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
class UPRInventoryViewModel;
class UPRWeaponComponent;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryItemUseRequestedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRInventoryItemDropRequestedSignature, FName, ItemId, int32, Count);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryItemEquipRequestedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRInventoryQuickbarAssignRequestedSignature, int32, SlotIndex, FGuid, InstanceGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryQuickbarClearRequestedSignature, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRInventoryPrimaryUnequipRequestedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryEquipmentUnequipRequestedSignature, FName, SlotId);

UENUM(BlueprintType)
enum class EPRInventoryConfirmationAction : uint8
{
	None,
	Drop
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRInventoryConfirmationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Confirmation")
	EPRInventoryConfirmationAction Action = EPRInventoryConfirmationAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Confirmation")
	FPRItemInstance Item;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Confirmation")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Confirmation")
	FText Prompt;

	bool IsValid() const { return Action != EPRInventoryConfirmationAction::None && Item.HasValidIdentity() && Count > 0; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryConfirmationRequestedSignature, const FPRInventoryConfirmationRequest&, Request);

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

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void BindViewModel(UPRInventoryViewModel* InViewModel);

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	UPRInventoryViewModel* GetBoundViewModel() const { return BoundViewModel.Get(); }

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

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void ShowBackpackPresentation();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void ShowWarehousePresentation();

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void ChangePresentationPage(int32 Delta);

	/** Cycles the stable presentation sort without changing authoritative container order. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void CyclePresentationSort();

	/** Cycles the item-type presentation filter; warehouse actions remain unavailable outside the ship lobby. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void CyclePresentationFilter();

	/** Shared mouse/controller navigation helper for native and Blueprint inventory panels. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void MovePresentationFocus(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Warehouse", meta = (ClampMin = "1"))
	void RequestTransferSelectedItem(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Confirmation", meta = (ClampMin = "1"))
	void BeginDropConfirmationSelectedItem(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Confirmation")
	void ConfirmPendingInventoryAction(bool bAccepted);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Confirmation")
	void CancelPendingInventoryAction() { ConfirmPendingInventoryAction(false); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Confirmation")
	FPRInventoryConfirmationRequest GetPendingConfirmation() const { return PendingConfirmation; }

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

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Confirmation")
	FPRInventoryConfirmationRequestedSignature OnConfirmationRequested;

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

	UFUNCTION()
	void HandleViewModelChanged();

	FReply HandleItemClicked(int32 ItemIndex);
	FReply HandleUseSelectedClicked();
	FReply HandleDropSelectedClicked();
	FReply HandleEquipSelectedClicked();
	FReply HandleUnequipPrimaryClicked();
	FReply HandleUnequipEquipmentSlotClicked(FName SlotId);
	FReply HandleShowBackpackClicked();
	FReply HandleShowWarehouseClicked();
	FReply HandlePreviousPageClicked();
	FReply HandleNextPageClicked();
	FReply HandleCycleSortClicked();
	FReply HandleCycleFilterClicked();
	FReply HandleTransferSelectedClicked();
	FReply HandleConfirmDropClicked();
	FReply HandleCancelDropClicked();
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
	bool IsPresentingWarehouse() const;

	TWeakObjectPtr<UPRInventoryComponent> BoundInventory;
	TWeakObjectPtr<UPRInventoryViewModel> BoundViewModel;
	FPRInventoryConfirmationRequest PendingConfirmation;

	UPROPERTY(Transient)
	TArray<FPRItemInstance> DisplayedItems;

	int32 SelectedItemIndex = INDEX_NONE;

	TSharedPtr<SScrollBox> ItemScrollBox;
	TSharedPtr<STextBlock> DetailTextBlock;
	TSharedPtr<STextBlock> ShipResourceSummaryTextBlock;
	TSharedPtr<STextBlock> PrimaryWeaponSummaryTextBlock;
	TSharedPtr<STextBlock> EquipmentSummaryTextBlock;
};
