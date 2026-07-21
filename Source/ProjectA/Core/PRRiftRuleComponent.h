#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "Progression/PRRiftRuleTypes.h"
#include "PRRiftRuleComponent.generated.h"

class UPRMissionModifierDefinitionDataAsset;
class APRPlayerState;
class APREnemyCharacter;
class UAbilitySystemComponent;
struct FPRMissionDefinition;

/** Server-owned mission rules.  GameState replicates compact snapshots; this component never trusts clients. */
UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRRiftRuleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRRiftRuleComponent();

	bool InitializeRules(const FPRMissionDefinition& MissionDefinition, FString* OutDiagnostic = nullptr);
	void ResetRules(float InitialStability);

	bool AdvanceStability(float DeltaSeconds, float BaseDrainPerSecond);
	bool ApplyObjectiveStageCost(float Cost);
	bool ReportAlarm(FName AlarmId, int32 Severity, float CostPerSeverity);
	void SetStability(float NewStability);

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	const FPRMissionRuleSnapshot& GetRuleSnapshot() const { return RuleSnapshot; }

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	const FPRRiftRiskSnapshot& GetRiskSnapshot() const { return RiskSnapshot; }

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	const TArray<FPRMissionModifierSummary>& GetModifierSummaries() const { return ModifierSummaries; }

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	bool HasModifier(FName ModifierId) const;

	UFUNCTION(BlueprintPure, Category = "Rift|Rules")
	float GetWorldMaterialLootMultiplier() const { return RuleSnapshot.WorldMaterialLootMultiplier; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Rules")
	bool ApplyRulesToPlayer(APRPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Rules")
	bool ApplyRulesToEnemy(APREnemyCharacter* Enemy);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Rules")
	void ClearAppliedEffects();

private:
	bool RebuildRules(const FPRMissionDefinition& MissionDefinition, FString* OutDiagnostic);
	void RefreshRiskSnapshot();
	const FPRRiftRiskBandDefinition* FindRiskBand(float Stability) const;
	static float ClampRuleMultiplier(float Value);
	bool ApplyEffectsToAbilitySystem(UAbilitySystemComponent* AbilitySystem, bool bPlayer);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPRMissionModifierDefinitionDataAsset>> ActiveModifiers;

	UPROPERTY(Transient)
	FPRMissionRuleSnapshot RuleSnapshot;

	UPROPERTY(Transient)
	FPRRiftRiskSnapshot RiskSnapshot;

	UPROPERTY(Transient)
	TArray<FPRMissionModifierSummary> ModifierSummaries;

	TSet<FName> ProcessedAlarmIds;
	TMap<TObjectKey<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>> AppliedEffectHandles;
	bool bInitialized = false;
};
