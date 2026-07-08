#pragma once

#include "CoreMinimal.h"
#include "PRRiftSettlementTypes.generated.h"

UENUM(BlueprintType)
enum class EPRRiftMissionResult : uint8
{
	None UMETA(DisplayName = "None"),
	Success UMETA(DisplayName = "Success"),
	Failed UMETA(DisplayName = "Failed")
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRiftSettlementData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement")
	EPRRiftMissionResult Result = EPRRiftMissionResult::None;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0.0"))
	float MissionTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float RiftStability = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ObjectiveProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 AlivePlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 ExtractedPlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 KilledEnemyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 ExtractedItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 ExtractedUniqueItemTypes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 ExtractedResourceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 ExtractedUniqueResourceTypes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rift|Settlement", meta = (ClampMin = "0"))
	int32 LostResourceCount = 0;

	bool IsValid() const
	{
		return Result != EPRRiftMissionResult::None;
	}
};
