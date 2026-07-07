#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/PRItemTypes.h"
#include "PRLootTableDataAsset.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FPRLootTableEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FPRItemInstance Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	bool IsValid() const
	{
		return Item.IsValid() && Weight > 0.0f;
	}
};

/**
 * First-pass weighted loot table for server-owned pickup spawning.
 */
UCLASS(BlueprintType)
class PROJECTA_API UPRLootTableDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPRLootTableDataAsset();

	static const FPrimaryAssetType LootTablePrimaryAssetType;
	static TArray<FPRLootTableEntry> MakeDefaultTestEntries();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "Loot")
	bool IsValidLootTable() const;

	UFUNCTION(BlueprintPure, Category = "Loot")
	float GetTotalWeight() const;

	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool RollLoot(float Roll, FPRItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool RollRandomLoot(FPRItemInstance& OutItem) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FPRLootTableEntry> Entries;
};
