#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/PRItemTypes.h"
#include "Items/PRItemUseTypes.h"
#include "PRItemDataAsset.generated.h"

class UGameplayEffect;
class UTexture2D;

UCLASS(BlueprintType)
class PROJECTA_API UPRItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ItemPrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AssetRegistrySearchable))
	FName ItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EPRItemType ItemType = EPRItemType::Consumable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bCanUse = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bCanEquip = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bCanDrop = true;

	/** Reserved for the later crafting system; task-critical items set this false now. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bCanCraft = true;

	/** Mission-local task carriers are removed before the settlement snapshot is persisted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bAutoProcessAtMissionEnd = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	FPRItemUseDefinition UseDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<UGameplayEffect> UseEffect;

	UFUNCTION(BlueprintPure, Category = "Items")
	bool IsStackable() const;
};
