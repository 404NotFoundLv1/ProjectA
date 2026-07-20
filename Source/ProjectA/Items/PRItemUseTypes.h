#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PRItemUseTypes.generated.h"

UENUM(BlueprintType)
enum class EPRItemUseKind : uint8
{
	None,
	RestoreHealth,
	RestoreShield,
	RestoreEnergy,
	Purify,
	GrantAmmo,
	MissionTool
};

/** Data-only behavior for one usable item definition. Inventory owns no attribute mutation. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemUseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	EPRItemUseKind Kind = EPRItemUseKind::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	bool bQuickbarEligible = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use", meta = (ClampMin = "0.0"))
	float UseDurationSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	FGameplayTag CooldownTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use", meta = (ClampMin = "0"))
	int32 GrantedItemCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Use")
	FName GrantedItemId = NAME_None;

	bool IsUsableFromQuickbar() const
	{
		return bQuickbarEligible && Kind != EPRItemUseKind::None && Kind != EPRItemUseKind::MissionTool
			&& FMath::IsFinite(UseDurationSeconds) && UseDurationSeconds >= 0.0f
			&& FMath::IsFinite(CooldownSeconds) && CooldownSeconds >= 0.0f;
	}
};
