#pragma once

#include "CoreMinimal.h"
#include "Items/PRInventoryComponent.h"
#include "PRWarehouseComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRWarehouseChangedDelegate);

/** Persistent, owner-only ship storage. It reuses the inventory FastArray contract but has no UI authority. */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRWarehouseComponent : public UPRInventoryComponent
{
	GENERATED_BODY()

public:
	UPRWarehouseComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Warehouse")
	FPRWarehouseChangedDelegate OnWarehouseChanged;

	UFUNCTION(BlueprintPure, Category = "Warehouse")
	TArray<FPRItemInstance> GetWarehouseItems() const { return GetInventoryItems(); }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Warehouse")
	bool ReplaceWarehouseItems(const TArray<FPRItemInstance>& Items) { return ReplaceInventoryItems(Items); }

	UFUNCTION(BlueprintPure, Category = "Warehouse")
	bool CanStoreItem(const FPRItemInstance& Item) const { return CanAddItem(Item); }

private:
	UFUNCTION()
	void HandleInventoryChanged();
};
