#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRStatusEffectTypes.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Core/PREncounterDirectorTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PREnemyDefinitionDataAsset.generated.h"

class UGameplayAbility;
class UPRLootTableDataAsset;

UENUM(BlueprintType)
enum class EPREnemyTier : uint8
{
	Regular,
	Elite
};

UENUM(BlueprintType)
enum class EPREnemyTargetPolicy : uint8
{
	Nearest,
	LowestHealth,
	Isolated,
	AllySupport
};

UENUM(BlueprintType)
enum class EPREnemyActionKind : uint8
{
	Melee,
	Projectile,
	Burst,
	Exploder,
	Burrow,
	Pounce,
	ShieldPulse,
	Summon,
	Disrupt
};

UENUM(BlueprintType)
enum class EPREnemyActionInterruptPolicy : uint8
{
	HeavyOrStun,
	AnyStaggerOrStun
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREnemyAttributeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") float MaxHealth = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") float InitialShield = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") float MaxShield = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") float AttackPower = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") float MoveSpeed = 360.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREnemyActionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") FGameplayTag ActionTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") EPREnemyActionKind Kind = EPREnemyActionKind::Melee;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") TSubclassOf<UGameplayAbility> AbilityClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float MinimumRange = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float MaximumRange = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float CooldownSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float TelegraphSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float BaseDamage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float Radius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") float ProjectileSpeed = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") FPRHitReactionDefinition HitReaction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") FPRTargetStatusEffectDefinition StatusEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") FName SummonDefinitionId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") int32 SummonCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Action") EPREnemyActionInterruptPolicy InterruptPolicy = EPREnemyActionInterruptPolicy::HeavyOrStun;
};

UCLASS(BlueprintType)
class PROJECTA_API UPREnemyDefinitionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType EnemyPrimaryAssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool ValidateDefinition(FString& OutDiagnostic) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FName EnemyId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") EPREnemyTier Tier = EPREnemyTier::Regular;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") EPREncounterUnitCategory EncounterCategory = EPREncounterUnitCategory::Melee;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.1")) float ThreatCost = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") EPREnemyTargetPolicy TargetPolicy = EPREnemyTargetPolicy::Nearest;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FPREnemyAttributeDefinition Attributes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") TArray<FPREnemyActionDefinition> Actions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FGameplayTagContainer ImmunityTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") bool bHeavyHitReactionsOnly = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") TObjectPtr<UPRLootTableDataAsset> LootTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") bool bDropsLoot = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Visual") FLinearColor PlaceholderColor = FLinearColor::White;
};
