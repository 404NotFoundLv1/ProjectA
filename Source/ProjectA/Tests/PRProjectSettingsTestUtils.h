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
		WaveInterval = Settings->WaveInterval;
		ExtractionRadius = Settings->ExtractionRadius;
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
		Settings->WaveInterval = WaveInterval;
		Settings->ExtractionRadius = ExtractionRadius;
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
	float WaveInterval = 0.0f;
	float ExtractionRadius = 0.0f;
	float AttackPowerDivisor = 0.0f;
	float MaxPollutionDamageReduction = 0.0f;
};
}
