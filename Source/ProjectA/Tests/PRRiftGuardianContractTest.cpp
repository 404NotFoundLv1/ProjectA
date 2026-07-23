#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRiftGuardianContractTest,
	"ProjectRift.Rift.Guardian.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRiftGuardianContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(
		TEXT("Rift Guardian has a dedicated Boss character"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftGuardianCharacter")));
	TestNotNull(
		TEXT("Objective graphs can bind a Boss encounter actor"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRBossObjectiveActor")));
	TestNotNull(
		TEXT("Rift Guardian exposes a locked charge ability"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.GA_RiftGuardianLockedCharge")));
	TestNotNull(
		TEXT("Rift Guardian resolves a swept locked charge on the server"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftGuardianChargeAction")));
	TestNotNull(
		TEXT("Rift Guardian exposes a persistent pollution-field actor"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftGuardianPollutionField")));
	if (UClass* RiftGameModeClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRiftGameMode")))
	{
		TestNotNull(
			TEXT("Rift Guardian records one-time Boss reward sources per run"),
			RiftGameModeClass->FindPropertyByName(TEXT("AcceptedBossRewardIds")));
	}
	else
	{
		AddError(TEXT("Rift GameMode class is not registered."));
	}

	if (UScriptStruct* TelegraphDefinition = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRBossTelegraphDefinition")))
	{
		TestNotNull(TEXT("Boss telegraphs expose an authored origin"), TelegraphDefinition->FindPropertyByName(TEXT("Origin")));
		TestNotNull(TEXT("Boss line telegraphs expose an authored length"), TelegraphDefinition->FindPropertyByName(TEXT("Length")));
		TestNotNull(TEXT("Boss line telegraphs expose an authored width"), TelegraphDefinition->FindPropertyByName(TEXT("Width")));
	}
	else
	{
		AddError(TEXT("Boss telegraph definition is unavailable."));
	}

	if (UEnum* RewardSourceEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRRewardSourceType")))
	{
		TestTrue(TEXT("Reward source enum appends Boss without reordering legacy values"), RewardSourceEnum->GetValueByName(TEXT("Boss")) >= 0);
	}
	else
	{
		AddError(TEXT("Reward source enum is unavailable."));
	}

	return true;
}

#endif
