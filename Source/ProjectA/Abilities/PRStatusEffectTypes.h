#pragma once

#include "CoreMinimal.h"
#include "PRStatusEffectTypes.generated.h"

class UPRStatusGameplayEffect;

/** Runtime parameters for one target status carried by an ability class. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRTargetStatusEffectDefinition
{
	GENERATED_BODY()

	FPRTargetStatusEffectDefinition() = default;

	FPRTargetStatusEffectDefinition(
		TSubclassOf<UPRStatusGameplayEffect> InEffectClass,
		const float InMagnitude,
		const float InDurationSeconds)
		: EffectClass(InEffectClass)
		, Magnitude(InMagnitude)
		, DurationSeconds(InDurationSeconds)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status")
	TSubclassOf<UPRStatusGameplayEffect> EffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status")
	float Magnitude = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Status", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DurationSeconds = 0.0f;
};
