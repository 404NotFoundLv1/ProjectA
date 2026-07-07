#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameState.h"
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
	int32 GetAlivePlayerCount() const { return AlivePlayerCount; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	float GetMissionTime() const { return MissionTime; }

	UFUNCTION(BlueprintPure, Category = "Rift|State")
	float GetObjectiveProgress() const { return ObjectiveProgress; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetCurrentObjectiveState(EPRRiftObjectiveState InCurrentObjectiveState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetRiftStability(float InRiftStability);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetExtractionAvailable(bool bInExtractionAvailable);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetAlivePlayerCount(int32 InAlivePlayerCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetMissionTime(float InMissionTime);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|State")
	void SetObjectiveProgress(float InObjectiveProgress);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentObjectiveState, BlueprintReadOnly, Category = "Rift|State")
	EPRRiftObjectiveState CurrentObjectiveState = EPRRiftObjectiveState::NotStarted;

	UPROPERTY(ReplicatedUsing = OnRep_RiftStability, BlueprintReadOnly, Category = "Rift|State")
	float RiftStability = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ExtractionAvailable, BlueprintReadOnly, Category = "Rift|State")
	bool bExtractionAvailable = false;

	UPROPERTY(ReplicatedUsing = OnRep_AlivePlayerCount, BlueprintReadOnly, Category = "Rift|State")
	int32 AlivePlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MissionTime, BlueprintReadOnly, Category = "Rift|State")
	float MissionTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveProgress, BlueprintReadOnly, Category = "Rift|State")
	float ObjectiveProgress = 0.0f;

	UFUNCTION()
	void OnRep_CurrentObjectiveState();

	UFUNCTION()
	void OnRep_RiftStability();

	UFUNCTION()
	void OnRep_ExtractionAvailable();

	UFUNCTION()
	void OnRep_AlivePlayerCount();

	UFUNCTION()
	void OnRep_MissionTime();

	UFUNCTION()
	void OnRep_ObjectiveProgress();
};
