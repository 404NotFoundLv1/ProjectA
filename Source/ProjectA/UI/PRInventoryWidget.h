#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/PRItemTypes.h"
#include "PRInventoryWidget.generated.h"

class SScrollBox;
class STextBlock;
class UPRInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryItemUseRequestedSignature, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRInventoryItemDropRequestedSignature, FName, ItemId, int32, Count);

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
	FText GetItemRarityText(EPRItemRarity Rarity) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FLinearColor GetItemRarityColor(EPRItemRarity Rarity) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	FText GetItemTooltipText(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RequestUseSelectedItem();

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RequestUseDisplayedItem(int32 ItemIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI", meta = (ClampMin = "1"))
	void RequestDropSelectedItem(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI", meta = (ClampMin = "1"))
	void RequestDropDisplayedItem(int32 ItemIndex, int32 Count = 1);

	UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
	FPRInventoryItemUseRequestedSignature OnUseItemRequested;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|UI")
	FPRInventoryItemDropRequestedSignature OnDropItemRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleInventoryChanged();

	FReply HandleItemClicked(int32 ItemIndex);
	FReply HandleUseSelectedClicked();
	FReply HandleDropSelectedClicked();
	void RebuildItemList();
	void RefreshSelectedItemDetails();
	FString BuildItemSummary(const FPRItemInstance& Item) const;
	FText BuildSelectedItemDetails() const;

	TWeakObjectPtr<UPRInventoryComponent> BoundInventory;

	UPROPERTY(Transient)
	TArray<FPRItemInstance> DisplayedItems;

	int32 SelectedItemIndex = INDEX_NONE;

	TSharedPtr<SScrollBox> ItemScrollBox;
	TSharedPtr<STextBlock> DetailTextBlock;
};
