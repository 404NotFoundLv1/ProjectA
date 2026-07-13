#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/PRItemTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "PRInventoryComponent.generated.h"

class UPRInventoryComponent;
class UPRItemDataAsset;
struct FPRInventoryList;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FPRItemInstance Item;
};

USTRUCT()
struct PROJECTA_API FPRInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPRInventoryEntry, FPRInventoryList>(Entries, DeltaParms, *this);
	}

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	void SetOwnerComponent(UPRInventoryComponent* InOwnerComponent);
	int32 FindEntryIndex(FName ItemId) const;

	UPROPERTY()
	TArray<FPRInventoryEntry> Entries;

private:
	UPRInventoryComponent* OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FPRInventoryList> : public TStructOpsTypeTraitsBase2<FPRInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPRInventoryChangedDelegate);

/**
 * Server-owned player inventory replicated to the owning client with FastArray.
 */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRInventoryComponent();

	virtual void PostInitProperties() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual ELifetimeCondition GetReplicationCondition() const override { return COND_OwnerOnly; }

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FPRInventoryChangedDelegate OnInventoryChanged;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(const FPRItemInstance& Item) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool AddItem(const FPRItemInstance& Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool UseItem(FName ItemId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ReplaceInventoryItems(const TArray<FPRItemInstance>& Items);

	const TArray<FPRItemInstance>& GetItems() const { return CachedItems; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FPRItemInstance> GetInventoryItems() const { return CachedItems; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Data")
	void SetItemDataAssets(const TArray<UPRItemDataAsset*>& InItemDataAssets);

	UFUNCTION(BlueprintPure, Category = "Inventory|Data")
	UPRItemDataAsset* FindItemData(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Data")
	int32 GetMaxStackCount(FName ItemId) const;

private:
	friend struct FPRInventoryList;

	int32 FindItemIndex(FName ItemId) const;
	int32 FindStackableEntryIndex(const FPRItemInstance& Item) const;
	void HandleInventoryListChanged();
	void NotifyInventoryChanged();
	void RefreshCachedItems();
	static bool CanStackItemInstances(const FPRItemInstance& ExistingItem, const FPRItemInstance& IncomingItem);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxSlots;

	UPROPERTY(Replicated)
	FPRInventoryList InventoryList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Data", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UPRItemDataAsset>> ItemDataAssets;

	UPROPERTY(Transient)
	TArray<FPRItemInstance> CachedItems;
};
