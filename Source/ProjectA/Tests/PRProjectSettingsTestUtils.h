#pragma once

#include "Settings/PRProjectSettings.h"

namespace ProjectRiftTests
{
class FScopedProjectSettingsOverride
{
public:
	FScopedProjectSettingsOverride()
		: Settings(GetMutableDefault<UPRProjectSettings>())
	{
		check(Settings);
		InitialRiftStability = Settings->InitialRiftStability;
		RiftStabilityDrainPerSecond = Settings->RiftStabilityDrainPerSecond;
		FailedResourceRetentionRate = Settings->FailedResourceRetentionRate;
		ReturnToLobbyDelayAfterSettlement = Settings->ReturnToLobbyDelayAfterSettlement;
		ObjectiveHoldDuration = Settings->ObjectiveHoldDuration;
		ObjectiveInteractionRadius = Settings->ObjectiveInteractionRadius;
		BaseEnemiesPerWave = Settings->BaseEnemiesPerWave;
		EnemiesPerAdditionalPlayer = Settings->EnemiesPerAdditionalPlayer;
		MaxAliveEnemies = Settings->MaxAliveEnemies;
		MaxAliveEnemiesPerAdditionalPlayer = Settings->MaxAliveEnemiesPerAdditionalPlayer;
		EnemyHealthMultiplierPerAdditionalPlayer = Settings->EnemyHealthMultiplierPerAdditionalPlayer;
		WaveInterval = Settings->WaveInterval;
		ExtractionRadius = Settings->ExtractionRadius;
		ReviveHoldDuration = Settings->ReviveHoldDuration;
		ReviveInteractionDistance = Settings->ReviveInteractionDistance;
		ReviveMovementCancelDistance = Settings->ReviveMovementCancelDistance;
		BleedOutDuration = Settings->BleedOutDuration;
		ReviveHealthFraction = Settings->ReviveHealthFraction;
		DroneReviveDuration = Settings->DroneReviveDuration;
		AttackPowerDivisor = Settings->AttackPowerDivisor;
		MaxPollutionDamageReduction = Settings->MaxPollutionDamageReduction;
	}

	~FScopedProjectSettingsOverride()
	{
		Settings->InitialRiftStability = InitialRiftStability;
		Settings->RiftStabilityDrainPerSecond = RiftStabilityDrainPerSecond;
		Settings->FailedResourceRetentionRate = FailedResourceRetentionRate;
		Settings->ReturnToLobbyDelayAfterSettlement = ReturnToLobbyDelayAfterSettlement;
		Settings->ObjectiveHoldDuration = ObjectiveHoldDuration;
		Settings->ObjectiveInteractionRadius = ObjectiveInteractionRadius;
		Settings->BaseEnemiesPerWave = BaseEnemiesPerWave;
		Settings->EnemiesPerAdditionalPlayer = EnemiesPerAdditionalPlayer;
		Settings->MaxAliveEnemies = MaxAliveEnemies;
		Settings->MaxAliveEnemiesPerAdditionalPlayer = MaxAliveEnemiesPerAdditionalPlayer;
		Settings->EnemyHealthMultiplierPerAdditionalPlayer = EnemyHealthMultiplierPerAdditionalPlayer;
		Settings->WaveInterval = WaveInterval;
		Settings->ExtractionRadius = ExtractionRadius;
		Settings->ReviveHoldDuration = ReviveHoldDuration;
		Settings->ReviveInteractionDistance = ReviveInteractionDistance;
		Settings->ReviveMovementCancelDistance = ReviveMovementCancelDistance;
		Settings->BleedOutDuration = BleedOutDuration;
		Settings->ReviveHealthFraction = ReviveHealthFraction;
		Settings->DroneReviveDuration = DroneReviveDuration;
		Settings->AttackPowerDivisor = AttackPowerDivisor;
		Settings->MaxPollutionDamageReduction = MaxPollutionDamageReduction;
	}

	FScopedProjectSettingsOverride(const FScopedProjectSettingsOverride&) = delete;
	FScopedProjectSettingsOverride& operator=(const FScopedProjectSettingsOverride&) = delete;

	UPRProjectSettings* operator->() const
	{
		return Settings;
	}

private:
	TObjectPtr<UPRProjectSettings> Settings;
	float InitialRiftStability = 0.0f;
	float RiftStabilityDrainPerSecond = 0.0f;
	float FailedResourceRetentionRate = 0.0f;
	float ReturnToLobbyDelayAfterSettlement = 0.0f;
	float ObjectiveHoldDuration = 0.0f;
	float ObjectiveInteractionRadius = 0.0f;
	int32 BaseEnemiesPerWave = 0;
	int32 EnemiesPerAdditionalPlayer = 0;
	int32 MaxAliveEnemies = 0;
	int32 MaxAliveEnemiesPerAdditionalPlayer = 0;
	float EnemyHealthMultiplierPerAdditionalPlayer = 0.0f;
	float WaveInterval = 0.0f;
	float ExtractionRadius = 0.0f;
	float ReviveHoldDuration = 0.0f;
	float ReviveInteractionDistance = 0.0f;
	float ReviveMovementCancelDistance = 0.0f;
	float BleedOutDuration = 0.0f;
	float ReviveHealthFraction = 0.0f;
	float DroneReviveDuration = 0.0f;
	float AttackPowerDivisor = 0.0f;
	float MaxPollutionDamageReduction = 0.0f;
};
}
