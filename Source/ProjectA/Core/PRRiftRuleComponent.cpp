#include "Core/PRRiftRuleComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Core/PRAssetManager.h"
#include "AbilitySystemComponent.h"
#include "Enemies/PREnemyCharacter.h"
#include "Player/PRPlayerState.h"
#include "Progression/PRMissionContractTypes.h"
#include "Progression/PRMissionModifierDefinitionDataAsset.h"
#include "Settings/PRProjectSettings.h"

UPRRiftRuleComponent::UPRRiftRuleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UPRRiftRuleComponent::InitializeRules(const FPRMissionDefinition& MissionDefinition, FString* OutDiagnostic)
{
	ClearAppliedEffects();
	if (!MissionDefinition.IsValid(OutDiagnostic))
	{
		return false;
	}
	return RebuildRules(MissionDefinition, OutDiagnostic);
}

void UPRRiftRuleComponent::ResetRules(const float InitialStability)
{
	ProcessedAlarmIds.Reset();
	RiskSnapshot = FPRRiftRiskSnapshot();
	RiskSnapshot.Stability = FMath::Clamp(InitialStability, 0.0f, 100.0f);
	RefreshRiskSnapshot();
}

bool UPRRiftRuleComponent::AdvanceStability(const float DeltaSeconds, const float BaseDrainPerSecond)
{
	if (!bInitialized || !FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(BaseDrainPerSecond)
		|| DeltaSeconds <= 0.0f || BaseDrainPerSecond <= 0.0f)
	{
		return false;
	}
	SetStability(RiskSnapshot.Stability - DeltaSeconds * BaseDrainPerSecond * RuleSnapshot.StabilityDrainMultiplier);
	return true;
}

bool UPRRiftRuleComponent::ApplyObjectiveStageCost(const float Cost)
{
	if (!bInitialized || !FMath::IsFinite(Cost) || Cost <= 0.0f)
	{
		return false;
	}
	SetStability(RiskSnapshot.Stability - Cost);
	return true;
}

bool UPRRiftRuleComponent::ReportAlarm(const FName AlarmId, const int32 Severity, const float CostPerSeverity)
{
	if (!bInitialized || AlarmId.IsNone() || ProcessedAlarmIds.Contains(AlarmId) || !FMath::IsFinite(CostPerSeverity) || CostPerSeverity <= 0.0f)
	{
		return false;
	}
	ProcessedAlarmIds.Add(AlarmId);
	SetStability(RiskSnapshot.Stability - CostPerSeverity * FMath::Clamp(Severity, 1, 3));
	return true;
}

void UPRRiftRuleComponent::SetStability(const float NewStability)
{
	RiskSnapshot.Stability = FMath::Clamp(FMath::IsFinite(NewStability) ? NewStability : 0.0f, 0.0f, 100.0f);
	RefreshRiskSnapshot();
}

bool UPRRiftRuleComponent::HasModifier(const FName ModifierId) const
{
	return ActiveModifiers.ContainsByPredicate([ModifierId](const UPRMissionModifierDefinitionDataAsset* Modifier)
	{
		return Modifier && Modifier->ModifierId == ModifierId;
	});
}

bool UPRRiftRuleComponent::ApplyRulesToPlayer(APRPlayerState* PlayerState)
{
	return PlayerState && ApplyEffectsToAbilitySystem(PlayerState->GetProjectRiftAbilitySystemComponent(), true);
}

bool UPRRiftRuleComponent::ApplyRulesToEnemy(APREnemyCharacter* Enemy)
{
	return Enemy && ApplyEffectsToAbilitySystem(Enemy->GetProjectRiftAbilitySystemComponent(), false);
}

void UPRRiftRuleComponent::ClearAppliedEffects()
{
	for (const TPair<TObjectKey<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>>& Pair : AppliedEffectHandles)
	{
		if (UAbilitySystemComponent* AbilitySystem = Pair.Key.ResolveObjectPtr())
		{
			for (const FActiveGameplayEffectHandle Handle : Pair.Value)
			{
				if (Handle.IsValid())
				{
					AbilitySystem->RemoveActiveGameplayEffect(Handle);
				}
			}
		}
	}
	AppliedEffectHandles.Reset();
}

bool UPRRiftRuleComponent::ApplyEffectsToAbilitySystem(UAbilitySystemComponent* AbilitySystem, const bool bPlayer)
{
	if (!bInitialized || !AbilitySystem || !AbilitySystem->GetOwnerActor() || !AbilitySystem->GetOwnerActor()->HasAuthority())
	{
		return false;
	}
	const TObjectKey<UAbilitySystemComponent> Key(AbilitySystem);
	if (AppliedEffectHandles.Contains(Key))
	{
		return true;
	}
	TArray<FActiveGameplayEffectHandle> Handles;
	for (const UPRMissionModifierDefinitionDataAsset* Modifier : ActiveModifiers)
	{
		const TSubclassOf<UGameplayEffect> EffectClass = Modifier
			? (bPlayer ? Modifier->PlayerEffectClass : Modifier->EnemyEffectClass)
			: nullptr;
		if (!EffectClass)
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, AbilitySystem->MakeEffectContext());
		if (!Spec.IsValid())
		{
			ClearAppliedEffects();
			return false;
		}
		const FActiveGameplayEffectHandle Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!Handle.IsValid())
		{
			for (const FActiveGameplayEffectHandle AppliedHandle : Handles)
			{
				AbilitySystem->RemoveActiveGameplayEffect(AppliedHandle);
			}
			return false;
		}
		Handles.Add(Handle);
	}
	AppliedEffectHandles.Add(Key, MoveTemp(Handles));
	return true;
}

bool UPRRiftRuleComponent::RebuildRules(const FPRMissionDefinition& MissionDefinition, FString* OutDiagnostic)
{
	ActiveModifiers.Reset();
	ModifierSummaries.Reset();
	RuleSnapshot = FPRMissionRuleSnapshot();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const int32 MaxModifiers = Settings ? FMath::Clamp(Settings->MaxMissionModifiers, 1, 6) : 6;
	if (MissionDefinition.ModifierIds.Num() > MaxModifiers)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission exceeds the configured modifier limit."); }
		return false;
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	if (!AssetManager && !MissionDefinition.ModifierIds.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission modifiers require the ProjectRift asset manager."); }
		return false;
	}

	for (const FName ModifierId : MissionDefinition.ModifierIds)
	{
		UPRMissionModifierDefinitionDataAsset* Modifier = AssetManager ? AssetManager->LoadMissionModifierSync(ModifierId) : nullptr;
		FString ModifierDiagnostic;
		if (!Modifier || Modifier->ModifierId != ModifierId || !Modifier->IsValidModifierDefinition(&ModifierDiagnostic))
		{
			if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("Mission modifier '%s' is unavailable or invalid: %s"), *ModifierId.ToString(), *ModifierDiagnostic); }
			ActiveModifiers.Reset();
			ModifierSummaries.Reset();
			return false;
		}
		ActiveModifiers.Add(Modifier);
		FPRMissionModifierSummary& Summary = ModifierSummaries.AddDefaulted_GetRef();
		Summary.ModifierId = Modifier->ModifierId;
		Summary.DisplayName = Modifier->DisplayName;
		Summary.Description = Modifier->Description;
		Summary.IconPath = Modifier->Icon.ToSoftObjectPath();
		RuleSnapshot.StabilityDrainMultiplier = ClampRuleMultiplier(RuleSnapshot.StabilityDrainMultiplier * Modifier->StabilityDrainMultiplier);
		RuleSnapshot.EnemyBudgetMultiplier = ClampRuleMultiplier(RuleSnapshot.EnemyBudgetMultiplier * Modifier->EnemyBudgetMultiplier);
		RuleSnapshot.EliteHealthMultiplier = ClampRuleMultiplier(RuleSnapshot.EliteHealthMultiplier * Modifier->EliteHealthMultiplier);
		RuleSnapshot.EliteAttackMultiplier = ClampRuleMultiplier(RuleSnapshot.EliteAttackMultiplier * Modifier->EliteAttackMultiplier);
		RuleSnapshot.PollutionDamageMultiplier = ClampRuleMultiplier(RuleSnapshot.PollutionDamageMultiplier * Modifier->PollutionDamageMultiplier);
		RuleSnapshot.RewardMultiplier = ClampRuleMultiplier(RuleSnapshot.RewardMultiplier * Modifier->RewardMultiplier);
		RuleSnapshot.WorldMaterialLootMultiplier = ClampRuleMultiplier(RuleSnapshot.WorldMaterialLootMultiplier * Modifier->WorldMaterialLootMultiplier);
		RuleSnapshot.GravityScale = ClampRuleMultiplier(RuleSnapshot.GravityScale * Modifier->GravityScale);
	}
	bInitialized = true;
	RefreshRiskSnapshot();
	return true;
}

void UPRRiftRuleComponent::RefreshRiskSnapshot()
{
	const FPRRiftRiskBandDefinition* Band = FindRiskBand(RiskSnapshot.Stability);
	if (!Band)
	{
		RiskSnapshot.CurrentTier = EPRRiftRiskTier::Stable;
		RiskSnapshot.EnemyBudgetMultiplier = RuleSnapshot.EnemyBudgetMultiplier;
		RiskSnapshot.RewardMultiplier = RuleSnapshot.RewardMultiplier;
		RiskSnapshot.EnvironmentalPollutionDamage = 0.0f;
		RiskSnapshot.EnvironmentalPulseIntervalSeconds = 0.0f;
		return;
	}
	RiskSnapshot.CurrentTier = Band->Tier;
	if (static_cast<uint8>(RiskSnapshot.CurrentTier) > static_cast<uint8>(RiskSnapshot.PeakTier))
	{
		RiskSnapshot.PeakTier = RiskSnapshot.CurrentTier;
	}
	RiskSnapshot.EnemyBudgetMultiplier = ClampRuleMultiplier(Band->EnemyBudgetMultiplier * RuleSnapshot.EnemyBudgetMultiplier);
	RiskSnapshot.RewardMultiplier = ClampRuleMultiplier(Band->RewardMultiplier * RuleSnapshot.RewardMultiplier);
	RiskSnapshot.PeakRewardMultiplier = FMath::Max(RiskSnapshot.PeakRewardMultiplier, RiskSnapshot.RewardMultiplier);
	RiskSnapshot.EnvironmentalPollutionDamage = FMath::Max(0.0f, Band->EnvironmentalPollutionDamage);
	RiskSnapshot.EnvironmentalPulseIntervalSeconds = FMath::Max(0.0f, Band->EnvironmentalPulseIntervalSeconds);
}

const FPRRiftRiskBandDefinition* UPRRiftRuleComponent::FindRiskBand(const float Stability) const
{
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	if (!Settings)
	{
		return nullptr;
	}
	const FPRRiftRiskBandDefinition* Best = nullptr;
	for (const FPRRiftRiskBandDefinition& Band : Settings->RiftRiskBands)
	{
		if (Band.IsValid() && Stability >= Band.MinimumStability && (!Best || Band.MinimumStability > Best->MinimumStability))
		{
			Best = &Band;
		}
	}
	return Best;
}

float UPRRiftRuleComponent::ClampRuleMultiplier(const float Value)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : 1.0f, 0.25f, 3.0f);
}
