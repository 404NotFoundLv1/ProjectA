#pragma once

#include "CoreMinimal.h"
#include "PRItemTypes.generated.h"

UENUM(BlueprintType)
enum class EPRItemType : uint8
{
	Consumable,
	Equipment,
	Material,
	QuestItem
};

UENUM(BlueprintType)
enum class EPRItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EPRItemRarity Rarity = EPRItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Durability = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<FName> Affixes;

	bool IsValid() const
	{
		return !ItemId.IsNone() && Count > 0;
	}
};
