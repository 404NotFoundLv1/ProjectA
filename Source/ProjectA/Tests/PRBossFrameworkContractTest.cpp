#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRBossFrameworkContractTest,
	"ProjectRift.Rift.BossFramework.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRBossFrameworkContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(
		TEXT("Boss definitions are reflected primary data assets"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossDefinitionDataAsset")));
	TestNotNull(
		TEXT("Boss encounters are reflected actors"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossEncounterController")));
	if (UClass* BossEncounterClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossEncounterController")))
	{
		TestNotNull(
			TEXT("Boss auto-start exposes a configurable player-join grace period"),
			BossEncounterClass->FindPropertyByName(TEXT("AutoStartDelaySeconds")));
	}
	TestNotNull(
		TEXT("Boss scheduler is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossSchedulerComponent")));
	TestNotNull(
		TEXT("Boss weak point component is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossWeakPointComponent")));
	TestNotNull(
		TEXT("Boss character is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossCharacter")));
	TestNotNull(
		TEXT("Boss ability base is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossGameplayAbility")));
	TestNotNull(
		TEXT("Boss targeted damage ability is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.GA_BossTargetedDamage")));
	TestNotNull(
		TEXT("Boss HUD widget is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossHUDWidget")));
	TestNotNull(
		TEXT("Boss telegraph actor is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossTelegraphActor")));
	TestNotNull(
		TEXT("Boss runtime snapshot is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRBossRuntimeSnapshot")));
	if (UClass* RiftGameStateClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftGameState")))
	{
		TestNotNull(
			TEXT("Rift game state replicates the compact boss snapshot"),
			RiftGameStateClass->FindPropertyByName(TEXT("BossRuntimeSnapshot")));
		TestNotNull(
			TEXT("Rift game state exposes boss snapshot access"),
			RiftGameStateClass->FindFunctionByName(TEXT("GetBossRuntimeSnapshot")));
	}
	else
	{
		AddError(TEXT("Rift game state class is unavailable."));
	}
	return true;
}

#endif
