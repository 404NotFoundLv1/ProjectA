#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Progression/PRRiftRuleTypes.h"
#include "PRProjectSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="ProjectRift Gameplay"))
class PROJECTA_API UPRProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPRProjectSettings();

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float InitialRiftStability = 100.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float RiftStabilityDrainPerSecond = 0.15f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float ObjectiveStageStabilityCost = 4.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float AlarmStabilityCostPerSeverity = 8.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="1", ClampMax="6", UIMin="1", UIMax="6"))
	int32 MaxMissionModifiers = 6;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission")
	TArray<FPRRiftRiskBandDefinition> RiftRiskBands;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float FailedResourceRetentionRate = 0.5f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float ReturnToLobbyDelayAfterSettlement = 4.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float SettlementAcknowledgementTimeout = 8.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="0.1", UIMin="0.1"))
	float ObjectiveHoldDuration = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Objective", meta=(ClampMin="1.0", UIMin="1.0"))
	float ObjectiveInteractionRadius = 250.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0", UIMin="0"))
	int32 BaseEnemiesPerWave = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0", UIMin="0"))
	int32 EnemiesPerAdditionalPlayer = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="1", UIMin="1"))
	int32 MaxAliveEnemies = 8;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0", UIMin="0"))
	int32 MaxAliveEnemiesPerAdditionalPlayer = 2;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.0", UIMin="0.0"))
	float EnemyHealthMultiplierPerAdditionalPlayer = 0.25f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.1", UIMin="0.1"))
	float WaveInterval = 6.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterSampleInterval = 1.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterBaseThreatBudget = 4.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterThreatPerAdditionalPlayer = 2.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="1.0")) float EncounterMaximumThreatBudget = 16.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterMinimumPlayerDistance = 600.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterPressureDuration = 20.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterCooldownDuration = 8.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterReinforcementInterval = 6.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterTelegraphDuration = 1.5f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterUnsafeRetryDelay = 2.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterRespiteDuration = 12.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.1")) float EncounterRespiteCooldown = 45.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0", ClampMax="1.0")) float EncounterLowHealthThreshold = 0.35f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0", ClampMax="1.0")) float EncounterPostRespiteBudgetFloor = 0.5f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="1")) int32 EncounterMaximumCandidateAttempts = 8;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterNavProjectionRadius = 250.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0")) float EncounterVisibilityCheckDistance = 5000.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0.0", ClampMax="180.0")) float EncounterVisibilityConeHalfAngleDegrees = 50.0f;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0")) int32 EncounterSoloEliteCap = 1;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0")) int32 EncounterLargePartyEliteCap = 2;
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Encounter", meta=(ClampMin="0")) int32 EncounterExploderCap = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Extraction", meta=(ClampMin="1.0", UIMin="1.0"))
	float ExtractionRadius = 320.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="0.1", UIMin="0.1"))
	float ReviveHoldDuration = 3.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="1.0", UIMin="1.0"))
	float ReviveInteractionDistance = 250.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="0.0", UIMin="0.0"))
	float ReviveMovementCancelDistance = 35.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="0.1", UIMin="0.1"))
	float BleedOutDuration = 30.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="0.01", ClampMax="1.0", UIMin="0.01", UIMax="1.0"))
	float ReviveHealthFraction = 0.4f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Revive", meta=(ClampMin="0.1", UIMin="0.1"))
	float DroneReviveDuration = 3.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1.0", UIMin="1.0"))
	float AttackPowerDivisor = 100.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float MaxPollutionDamageReduction = 0.8f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Diagnostics", meta=(ClampMin="100", ClampMax="5000", UIMin="100", UIMax="5000"))
	int32 DiagnosticEventCapacity = 500;
};
