#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "PRCombatFeedbackTypes.generated.h"

UENUM(BlueprintType)
enum class EPRHitReactionStrength : uint8
{
	None,
	Light,
	Heavy
};

UENUM(BlueprintType)
enum class EPRCombatFeedbackPolicy : uint8
{
	None,
	TargetOnly,
	TargetAndSource
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRHitReactionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Feedback")
	EPRHitReactionStrength Strength = EPRHitReactionStrength::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Feedback", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDamageRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	float BaseDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage")
	FGameplayTag DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Feedback")
	FHitResult HitResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Feedback")
	FPRHitReactionDefinition HitReaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Feedback")
	EPRCombatFeedbackPolicy FeedbackPolicy = EPRCombatFeedbackPolicy::None;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRResolvedDamage
{
	GENERATED_BODY()

	/** World-space direction from the damaged actor toward the original source. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	FVector IncomingDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	EPRHitReactionStrength HitReactionStrength = EPRHitReactionStrength::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	float ShieldDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	float HealthDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	bool bShieldBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	bool bLethal = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRHitConfirmation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	float ShieldDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	float HealthDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	bool bShieldBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
	bool bLethal = false;
};
