#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/PREncounterDirectorTypes.h"
#include "PREncounterDirectorComponent.generated.h"

class APRRiftObjectiveActor;
class APRSpawnManager;

UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPREncounterDirectorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPREncounterDirectorComponent();
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") const FPREncounterDirectorSnapshot& GetSnapshot() const { return Snapshot; }
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") float CalculateTargetThreatBudget(const FPREncounterScalingSnapshot& Scaling) const;
	UFUNCTION(BlueprintPure, Category = "Rift|Encounter") bool IsEncounterActive() const { return Snapshot.Phase != EPREncounterPhase::Inactive; }

	void SetEncounterManagers(const TArray<APRSpawnManager*>& InManagers);
	bool StartEncounter(APRRiftObjectiveActor* ObjectiveActor, int32 RunSeed);
	void StopEncounter();
	void TickEncounter();
private:
	FPREncounterScalingSnapshot BuildScalingSnapshot() const;
	void UpdateSnapshot(const FPREncounterScalingSnapshot& Scaling);
	bool TrySpawnReinforcement(const FPREncounterScalingSnapshot& Scaling, bool bImmediate);
	void SetDecision(FName DecisionCode, const FString& RejectionReason = FString());
	APRSpawnManager* SelectManager(const FPREncounterScalingSnapshot& Scaling) const;
	FPREncounterSpawnRequest BuildBasicRequest(const FPREncounterScalingSnapshot& Scaling, APRSpawnManager* Manager) const;
	bool CanSpawnCategory(EPREncounterUnitCategory Category, const FPREncounterScalingSnapshot& Scaling) const;
	int32 GetAliveCategoryCount(EPREncounterUnitCategory Category) const;
	float GetAliveThreat() const;
	int32 GetAliveEnemyCount() const;
	int32 GetDeterministicSeed(const FPREncounterScalingSnapshot& Scaling) const;
	UPROPERTY(Transient) FPREncounterDirectorSnapshot Snapshot;
	UPROPERTY(Transient) TObjectPtr<APRRiftObjectiveActor> ActiveObjective;
	UPROPERTY(Transient) TArray<TObjectPtr<APRSpawnManager>> EncounterManagers;
	int32 RunSeed = 0;
	int32 DecisionOrdinal = 0;
	float PressureEndsAt = 0.0f;
	float CooldownEndsAt = 0.0f;
	float RespiteEndsAt = 0.0f;
	float NextReinforcementAt = 0.0f;
	float NextRespiteAllowedAt = 0.0f;
	FTimerHandle EncounterTimerHandle;
};
