#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRAssetManager.h"
#include "Core/PRRiftRuleComponent.h"
#include "Progression/PRObjectiveGraphTypes.h"
#include "Progression/PRMissionModifierDefinitionDataAsset.h"
#include "Settings/PRProjectSettings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRiftRulesContractTest,
	"ProjectRift.Rift.Rules.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRiftRulesContractTest::RunTest(const FString& Parameters)
{
	UScriptStruct* ObjectiveNodeStruct = FPRObjectiveNodeDefinition::StaticStruct();
	TestNotNull(TEXT("Objective nodes expose a failure time limit"),
		ObjectiveNodeStruct ? ObjectiveNodeStruct->FindPropertyByName(TEXT("FailureTimeLimitSeconds")) : nullptr);
	TestNotNull(TEXT("Objective nodes expose an optional reward budget"),
		ObjectiveNodeStruct ? ObjectiveNodeStruct->FindPropertyByName(TEXT("RewardBudgetBonus")) : nullptr);

	UClass* ObjectiveRuntimeClass = UPRObjectiveGraphComponent::StaticClass();
	TestNotNull(TEXT("Objective graph runtime exposes a failure transition"),
		ObjectiveRuntimeClass ? ObjectiveRuntimeClass->FindFunctionByName(TEXT("FailNode")) : nullptr);
	TestNotNull(TEXT("Objective graph runtime exposes server time advancement"),
		ObjectiveRuntimeClass ? ObjectiveRuntimeClass->FindFunctionByName(TEXT("AdvanceTime")) : nullptr);

	TestNotNull(TEXT("Rift rule component is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftRuleComponent")));
	TestNotNull(TEXT("Mission modifier definition asset is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRMissionModifierDefinitionDataAsset")));
	TestNotNull(TEXT("Replicated rift risk snapshot is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRRiftRiskSnapshot")));

	UClass* SettingsClass = UPRProjectSettings::StaticClass();
	TestNotNull(TEXT("Project settings expose objective-stage stability cost"),
		SettingsClass ? SettingsClass->FindPropertyByName(TEXT("ObjectiveStageStabilityCost")) : nullptr);
	TestNotNull(TEXT("Project settings expose alarm stability cost"),
		SettingsClass ? SettingsClass->FindPropertyByName(TEXT("AlarmStabilityCostPerSeverity")) : nullptr);
	TestNotNull(TEXT("Project settings expose authored risk bands"),
		SettingsClass ? SettingsClass->FindPropertyByName(TEXT("RiftRiskBands")) : nullptr);

	UClass* RuleComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftRuleComponent"));
	TestNotNull(TEXT("Rift rule component exposes player modifier application"),
		RuleComponentClass ? RuleComponentClass->FindFunctionByName(TEXT("ApplyRulesToPlayer")) : nullptr);
	TestNotNull(TEXT("Rift rule component exposes spawned enemy modifier application"),
		RuleComponentClass ? RuleComponentClass->FindFunctionByName(TEXT("ApplyRulesToEnemy")) : nullptr);
	TestNotNull(TEXT("Rift rule component exposes deterministic effect cleanup"),
		RuleComponentClass ? RuleComponentClass->FindFunctionByName(TEXT("ClearAppliedEffects")) : nullptr);

	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	TestEqual(TEXT("Exactly four default rift risk bands are configured"), Settings ? Settings->RiftRiskBands.Num() : 0, 4);
	UPRRiftRuleComponent* Rules = NewObject<UPRRiftRuleComponent>(GetTransientPackage());
	TestNotNull(TEXT("Risk rule component can be constructed without a mission world"), Rules);
	if (Rules)
	{
		Rules->ResetRules(100.0f);
		TestEqual(TEXT("Full stability begins stable"), Rules->GetRiskSnapshot().CurrentTier, EPRRiftRiskTier::Stable);
		Rules->SetStability(49.0f);
		TestEqual(TEXT("Stability below 50 enters the critical band"), Rules->GetRiskSnapshot().CurrentTier, EPRRiftRiskTier::Critical);
		TestEqual(TEXT("Critical risk carries the authored 2-point pollution pulse"), Rules->GetRiskSnapshot().EnvironmentalPollutionDamage, 2.0f);
		Rules->SetStability(24.0f);
		TestEqual(TEXT("Stability below 25 enters collapse"), Rules->GetRiskSnapshot().CurrentTier, EPRRiftRiskTier::Collapse);
		TestEqual(TEXT("Peak risk persists after recovery"), Rules->GetRiskSnapshot().PeakTier, EPRRiftRiskTier::Collapse);
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	const TArray<FName> ModifierIds = {
		TEXT("Modifier.LowGravity"), TEXT("Modifier.ShieldInterference"), TEXT("Modifier.PollutionAmplified"),
		TEXT("Modifier.EliteReinforcements"), TEXT("Modifier.ResourceRich") };
	for (const FName ModifierId : ModifierIds)
	{
		UPRMissionModifierDefinitionDataAsset* Modifier = AssetManager ? AssetManager->LoadMissionModifierSync(ModifierId) : nullptr;
		TestTrue(FString::Printf(TEXT("Modifier asset %s is registered and valid"), *ModifierId.ToString()),
			Modifier && Modifier->ModifierId == ModifierId && Modifier->IsValidModifierDefinition());
	}

	return true;
}

#endif
