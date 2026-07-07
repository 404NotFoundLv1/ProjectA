#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/PRItemTypes.h"
#include "PRInventoryComponent.generated.h"

/**
 * Server-owned player inventory. Replication is added in a later inventory milestone.
 */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRInventoryComponent();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool AddItem(const FPRItemInstance& Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool UseItem(FName ItemId);

	const TArray<FPRItemInstance>& GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FPRItemInstance> GetInventoryItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemId) const;

private:
	int32 FindItemIndex(FName ItemId) const;
	void NotifyInventoryChanged();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxSlots;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FPRItemInstance> Items;
};
