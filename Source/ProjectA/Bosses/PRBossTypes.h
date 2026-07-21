#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRStatusEffectTypes.h"
#include "GameplayTagContainer.h"
#include "PRBossTypes.generated.h"

class UGameplayAbility;

UENUM(BlueprintType)
enum class EPRBossRuntimeState : uint8
{
	Idle,
	Selecting,
	Telegraphing,
	Executing,
	PhaseTransition,
	Staggered,
	Suspended,
	Defeated,
	Resetting
};

UENUM(BlueprintType)
enum class EPRBossTargetPolicy : uint8
{
	HighestThreat,
	Nearest,
	Farthest,
	LowestHealth,
	DeterministicRandomUnique,
	AllLiving
};

UENUM(BlueprintType)
enum class EPRBossTelegraphShape : uint8
{
	None,
	Circle,
	Line,
	Cone,
	Targeted
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossPlayerCountScalar
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling", meta = (ClampMin = "0.0")) float Solo = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling", meta = (ClampMin = "0.0")) float TwoPlayers = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling", meta = (ClampMin = "0.0")) float ThreePlayers = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling", meta = (ClampMin = "0.0")) float FourPlayers = 1.0f;

	float GetForPlayerCount(int32 PlayerCount) const
	{
		switch (FMath::Clamp(PlayerCount, 1, 4))
		{
		case 2: return TwoPlayers;
		case 3: return ThreePlayers;
		case 4: return FourPlayers;
		default: return Solo;
		}
	}
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossTargetingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting") EPRBossTargetPolicy Policy = EPRBossTargetPolicy::HighestThreat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting", meta = (ClampMin = "1")) int32 BaseTargetCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting", meta = (ClampMin = "0")) int32 AdditionalTargetsPerPlayer = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting", meta = (ClampMin = "0.0")) float MaximumRange = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossTelegraphDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph") EPRBossTelegraphShape Shape = EPRBossTelegraphShape::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (ClampMin = "0.0")) float DurationSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph", meta = (ClampMin = "0.0")) float Radius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph") FLinearColor Color = FLinearColor(1.0f, 0.15f, 0.05f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Telegraph") FGameplayTag CueTag;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossWeakPointDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint") FName WeakPointId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint", meta = (ClampMin = "1.0")) float DamageMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint", meta = (ClampMin = "0.0")) float InterruptDamageThreshold = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint") bool bOnlyWhileInterruptible = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint") FVector RelativeLocation = FVector(0.0f, 0.0f, 76.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint", meta = (ClampMin = "1.0")) float Radius = 42.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossArenaEventDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Arena") FName ArenaEventId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Arena") FGameplayTag EventTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Arena", meta = (ClampMin = "0.0")) float DurationSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossAbilityPatternDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FName PatternId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") TSubclassOf<UGameplayAbility> AbilityClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0")) float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0")) float CooldownSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0")) float ExecutionDurationSeconds = 0.35f;
	/** Damage is resolved exclusively through the existing server-authoritative combat library. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0")) float BaseDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0.0")) float EffectRadius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FGameplayTag DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FPRHitReactionDefinition HitReaction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") TArray<FPRTargetStatusEffectDefinition> TargetStatusEffects;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern", meta = (ClampMin = "0")) int32 SummonCount = 0;
	/** Existing encounter-roster enemy definition used by UGA_BossSummon. Required when SummonCount is positive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FName SummonDefinitionId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FPRBossTargetingDefinition Targeting;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FPRBossTelegraphDefinition Telegraph;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") bool bInterruptible = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern") FName ArenaEventId = NAME_None;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossPhaseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase") FName PhaseId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0")) float StartHealthPercent = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase") TArray<FName> EnabledPatternIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Phase") FGameplayTagContainer ImmunityTags;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") FName BossId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") EPRBossRuntimeState State = EPRBossRuntimeState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") int32 PhaseIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") FName PhaseId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") FName ActivePatternId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") FName PrimaryTargetId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") float HealthPercent = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") float TelegraphRemainingSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") bool bInterruptible = false;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") bool bStaggered = false;
	UPROPERTY(BlueprintReadOnly, Category = "Boss|Runtime") bool bVisible = false;
};
