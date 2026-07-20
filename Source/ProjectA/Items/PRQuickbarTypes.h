#pragma once

#include "CoreMinimal.h"
#include "Items/PRItemUseTypes.h"
#include "PRQuickbarTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FPRQuickSlotReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quickbar", meta = (ClampMin = "0", ClampMax = "3"))
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quickbar")
	FGuid InstanceGuid;

	bool IsValid() const { return SlotIndex >= 0 && SlotIndex < 4 && InstanceGuid.IsValid(); }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRQuickbarUseState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	FGuid InstanceGuid;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	float StartedServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	EPRItemUseKind Kind = EPRItemUseKind::None;

	/** A direct inventory use has no bound quick slot and therefore uses INDEX_NONE here. */
	bool IsActive() const { return TransactionId.IsValid() && InstanceGuid.IsValid(); }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRQuickbarCooldownState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	FGameplayTag CooldownTag;

	UPROPERTY(BlueprintReadOnly, Category = "Quickbar")
	float EndServerTime = 0.0f;

	bool IsValid() const { return CooldownTag.IsValid() && FMath::IsFinite(EndServerTime); }
};
