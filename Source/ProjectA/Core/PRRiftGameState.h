#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameState.h"
#include "Core/PRRiftSettlementTypes.h"
#include "Progression/PRObjectiveGraphTypes.h"
#include "PRRiftGameState.generated.h"

UENUM(BlueprintType)
enum class EPRRiftObjectiveState : uint8
{
	NotStarted UMETA(DisplayName = "Not Started"),
	Active UMETA(DisplayName = "Active"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed")
};

/**
 * Replicated rift mission state visible to every client.
 */
UCLASS()
class PROJECTA_API APRRiftGameState : public APRGameState
{
	GENERATED_BODY()

public:
	APRRiftGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	EPRRiftObjectiveState GetCurrentObjectiveState() const { return CurrentObjectiveState; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	float GetRiftStability() const { return RiftStability; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	bool IsExtractionAvailable() const { return bExtractionAvailable; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	int32 GetExtractedPlayerCount() const { return ExtractedPlayerCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	bool IsExtractionComplete() const { return bExtractionComplete; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	int32 GetAlivePlayerCount() const { return AlivePlayerCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	int32 GetDifficultyPlayerCount() const { return DifficultyPlayerCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	float GetMissionTime() const { return MissionTime; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	float GetObjectiveProgress() const { return ObjectiveProgress; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	int32 GetKilledEnemyCount() const { return KilledEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|ObjectiveGraph")
	const TArray<FPRObjectiveSummary>& GetObjectiveSummaries() const { return ObjectiveSummaries; }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	bool IsSettlementReady() const { return bSettlementReady; }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	FPRRiftSettlementData GetSettlementData() const { return SettlementData; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetCurrentObjectiveState(EPRRiftObjectiveState InCurrentObjectiveState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetRiftStability(float InRiftStability);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetExtractionAvailable(bool bInExtractionAvailable);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetExtractedPlayerCount(int32 InExtractedPlayerCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetExtractionComplete(bool bInExtractionComplete);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetAlivePlayerCount(int32 InAlivePlayerCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetDifficultyPlayerCount(int32 InDifficultyPlayerCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetMissionTime(float InMissionTime);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetObjectiveProgress(float InObjectiveProgress);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetKilledEnemyCount(int32 InKilledEnemyCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void IncrementKilledEnemyCount();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|ObjectiveGraph")
	void SetObjectiveSummaries(const TArray<FPRObjectiveSummary>& InObjectiveSummaries);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Settlement")
	void SetSettlementData(const FPRRiftSettlementData& InSettlementData);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Settlement")
	void SetSettlementReady(bool bInSettlementReady);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Settlement")
	void ResetSettlementData();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveState, BlueprintReadOnly, Category = "Rift|State")
	EPRRiftObjectiveState CurrentObjectiveState = EPRRiftObjectiveState::NotStarted;

	UPROPERTY(ReplicatedUsing = OnRep_RiftStability, BlueprintReadOnly, Category = "Rift|State")
	float RiftStability = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractionAvailable, BlueprintReadOnly, Category = "Rift|State")
	bool bExtractionAvailable = false;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractedPlayerCount, BlueprintReadOnly, Category = "Rift|State")
	int32 ExtractedPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractionComplete, BlueprintReadOnly, Category = "Rift|State")
	bool bExtractionComplete = false;

	UPROPERTY(ReplicatedUsing = OnRep_AlivePlayerCount, BlueprintReadOnly, Category = "Rift|State")
	int32 AlivePlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_DifficultyPlayerCount, BlueprintReadOnly, Category = "Rift|State")
	int32 DifficultyPlayerCount = 1;

	UPROPERTY(ReplicatedUsing = OnRep_MissionTime, BlueprintReadOnly, Category = "Rift|State")
	float MissionTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveProgress, BlueprintReadOnly, Category = "Rift|State")
	float ObjectiveProgress = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_KilledEnemyCount, BlueprintReadOnly, Category = "Rift|State")
	int32 KilledEnemyCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rift|ObjectiveGraph")
	TArray<FPRObjectiveSummary> ObjectiveSummaries;

	UPROPERTY(ReplicatedUsing = OnRep_SettlementData, BlueprintReadOnly, Category = "Rift|Settlement")
	FPRRiftSettlementData SettlementData;

	UPROPERTY(ReplicatedUsing = OnRep_SettlementReady, BlueprintReadOnly, Category = "Rift|Settlement")
	bool bSettlementReady = false;

	UFUNCTION()
	void OnRep_CurrentObjectiveState();

	UFUNCTION()
	void OnRep_RiftStability();

	UFUNCTION()
	void OnRep_ExtractionAvailable();

	UFUNCTION()
	void OnRep_ExtractedPlayerCount();

	UFUNCTION()
	void OnRep_ExtractionComplete();

	UFUNCTION()
	void OnRep_AlivePlayerCount();

	UFUNCTION()
	void OnRep_DifficultyPlayerCount();

	UFUNCTION()
	void OnRep_MissionTime();

	UFUNCTION()
	void OnRep_ObjectiveProgress();

	UFUNCTION()
	void OnRep_KilledEnemyCount();

	UFUNCTION()
	void OnRep_SettlementData();

	UFUNCTION()
	void OnRep_SettlementReady();
};
