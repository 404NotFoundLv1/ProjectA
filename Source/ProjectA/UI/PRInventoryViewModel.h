#pragma once

#include "CoreMinimal.h"
#include "Items/PRItemTransactionTypes.h"
#include "Items/PRItemTypes.h"
#include "UObject/Object.h"
#include "PRInventoryViewModel.generated.h"

class APRPlayerState;
class UPRInventoryComponent;
class UPRWarehouseComponent;
class UPREquipmentComponent;
class UPRWeaponComponent;
class UPRQuickbarComponent;
class UPRItemTransactionComponent;
class UTexture2D;

UENUM(BlueprintType)
enum class EPRInventoryPresentationSource : uint8
{
	Backpack,
	Warehouse
};

UENUM(BlueprintType)
enum class EPRInventorySortField : uint8
{
	AuthorityOrder,
	Name,
	Type,
	Rarity,
	Level
};

UENUM(BlueprintType)
enum class EPRInventoryFilter : uint8
{
	All,
	Equipment,
	Consumable,
	Tool,
	Ammunition
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRInventoryPresentationRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	FPRItemInstance Item;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	bool bFromWarehouse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	bool bEquippable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	bool bUsable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	bool bDroppable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	int32 QuickbarSlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRInventoryPageState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	int32 PageIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	int32 PageCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	int32 TotalRows = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	int32 PageSize = 24;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRInventoryComparison
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	FPRItemInstance Candidate;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	FPRItemInstance Equipped;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Presentation")
	TArray<FText> Differences;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRInventoryViewModelChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRInventoryViewModelTransactionDelegate, const FPRItemTransactionResult&, Result);

/** Local presentation adapter. It derives rows from replicated state and never owns authoritative inventory data. */
UCLASS(BlueprintType)
class PROJECTA_API UPRInventoryViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void BindPlayerState(APRPlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void UnbindPlayerState();

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	bool IsBound() const { return BoundPlayerState.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SetSource(EPRInventoryPresentationSource InSource);

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	EPRInventoryPresentationSource GetSource() const { return Source; }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SetSort(EPRInventorySortField InSortField, bool bInAscending);

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	EPRInventorySortField GetSortField() const { return SortField; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	bool IsSortAscending() const { return bAscending; }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SetFilter(EPRInventoryFilter InFilter, const FString& InSearchText);

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	EPRInventoryFilter GetFilter() const { return Filter; }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SetPageIndex(int32 InPageIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void ChangePage(int32 Delta) { SetPageIndex(PageIndex + Delta); }

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SelectInstance(FGuid InstanceGuid);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void SelectNextFocusable(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Presentation")
	void Refresh();

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	TArray<FPRInventoryPresentationRow> GetVisibleRows() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	FPRInventoryPageState GetPageState() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	FGuid GetSelectedInstanceGuid() const { return SelectedInstanceGuid; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	FPRInventoryPresentationRow GetSelectedRow() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	FPRInventoryComparison GetSelectedComparison() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Presentation")
	FPRItemTransactionResult GetLastTransactionResult() const { return LastTransactionResult; }

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Presentation")
	FPRInventoryViewModelChangedDelegate OnViewChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Presentation")
	FPRInventoryViewModelTransactionDelegate OnTransactionResult;

private:
	UFUNCTION()
	void HandleAuthoritativeStateChanged();

	UFUNCTION()
	void HandleTransactionResult(const FPRItemTransactionResult& Result);

	bool PassesFilter(const FPRItemInstance& Item) const;
	FPRInventoryPresentationRow BuildRow(const FPRItemInstance& Item) const;
	FText GetDisplayName(const FPRItemInstance& Item) const;
	int32 GetQuickbarSlotIndex(const FGuid& InstanceGuid) const;
	void NormalizePageAndSelection();

	TWeakObjectPtr<APRPlayerState> BoundPlayerState;
	TWeakObjectPtr<UPRInventoryComponent> BoundInventory;
	TWeakObjectPtr<UPRWarehouseComponent> BoundWarehouse;
	TWeakObjectPtr<UPREquipmentComponent> BoundEquipment;
	TWeakObjectPtr<UPRWeaponComponent> BoundWeapon;
	TWeakObjectPtr<UPRQuickbarComponent> BoundQuickbar;
	TWeakObjectPtr<UPRItemTransactionComponent> BoundTransactions;

	UPROPERTY(Transient)
	TArray<FPRInventoryPresentationRow> FilteredRows;

	EPRInventoryPresentationSource Source = EPRInventoryPresentationSource::Backpack;
	EPRInventorySortField SortField = EPRInventorySortField::AuthorityOrder;
	EPRInventoryFilter Filter = EPRInventoryFilter::All;
	bool bAscending = true;
	int32 PageIndex = 0;
	int32 PageSize = 24;
	FString SearchText;
	FGuid SelectedInstanceGuid;
	FPRItemTransactionResult LastTransactionResult;
};
