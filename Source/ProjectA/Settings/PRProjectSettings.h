#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PRProjectSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="ProjectRift Gameplay"))
class PROJECTA_API UPRProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0"))
	float InitialRiftStability = 100.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mission", meta=(ClampMin="0.0", UIMin="0.0"))
	float RiftStabilityDrainPerSecond = 1.0f;

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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.1", UIMin="0.1"))
	float WaveInterval = 6.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Extraction", meta=(ClampMin="1.0", UIMin="1.0"))
	float ExtractionRadius = 320.0f;
};
