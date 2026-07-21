#pragma once

#include "CoreMinimal.h"
#include "PREncounterDirectorTypes.generated.h"

class APREnemyCharacter;

/**
 * Geometry shared by encounter spawning and its regression tests. A navigation
 * projection denotes an agent's ground contact point, not the center of its
 * collision capsule.
 */
namespace ProjectRiftEncounterSpawn
{
	FORCEINLINE FVector GetCapsuleCenterForNavigationLocation(const FVector& NavigationLocation, const float CapsuleHalfHeight)
	{
		return NavigationLocation + FVector::UpVector * FMath::Max(0.0f, CapsuleHalfHeight);
	}
}

UENUM(BlueprintType)
enum class EPREncounterPhase : uint8 { Inactive, Pressure, Cooldown, Respite, Telegraph };

UENUM(BlueprintType)
enum class EPREncounterUnitCategory : uint8 { Melee, Ranged, Exploder, Elite };

USTRUCT(BlueprintType)
struct PROJECTA_API FPREncounterScalingSnapshot
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 FrozenPlayerCount = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 ActivePlayerCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 DownedPlayerCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float TeamHealthFraction = 1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") bool bAllStandingPlayersLowHealth = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float RiftStability = 100.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float RiskEnemyBudgetMultiplier = 1.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName ObjectiveNodeId = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName EncounterRegionId = NAME_None;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREncounterSpawnEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") FName EntryId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") TSubclassOf<APREnemyCharacter> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") float ThreatCost = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") EPREncounterUnitCategory Category = EPREncounterUnitCategory::Melee;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") float SelectionWeight = 1.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREncounterSpawnRequest
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName EntryId = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") TSubclassOf<APREnemyCharacter> EnemyClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") EPREncounterUnitCategory Category = EPREncounterUnitCategory::Melee;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float ThreatCost = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") bool bElite = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName SpawnGroupId = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName ObjectiveNodeId = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 DecisionOrdinal = 0;
	/** Stable mission seed supplied by the server director for reproducible candidate ordering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 RunSeed = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREncounterDirectorSnapshot
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") EPREncounterPhase Phase = EPREncounterPhase::Inactive;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float TargetThreatBudget = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float AliveThreat = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") int32 AliveEnemyCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") float NextEventRemainingSeconds = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName ObjectiveNodeId = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FName LastDecisionCode = NAME_None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter") FString LastRejectionReason;
};
