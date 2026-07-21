#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PREnemySpawnPoint.h"
#include "Core/PREncounterExclusionVolume.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRSpawnManager.h"
#include "Settings/PRProjectSettings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREncounterDirectorContractTest,
	"ProjectRift.Rift.EncounterDirector.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREncounterDirectorContractTest::RunTest(const FString& Parameters)
{
	UClass* DirectorClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREncounterDirectorComponent"));
	TestNotNull(TEXT("Server encounter director component is reflected"), DirectorClass);
	TestNotNull(TEXT("Encounter scaling snapshot is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREncounterScalingSnapshot")));
	TestNotNull(TEXT("Encounter director snapshot is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREncounterDirectorSnapshot")));
	TestNotNull(TEXT("Encounter spawn request is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREncounterSpawnRequest")));
	if (UScriptStruct* SpawnRequestStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREncounterSpawnRequest")))
	{
		TestNotNull(TEXT("Spawn request carries the reproducible run seed"), SpawnRequestStruct->FindPropertyByName(TEXT("RunSeed")));
	}
	TestNotNull(TEXT("Encounter exclusion volume is reflected"), APREncounterExclusionVolume::StaticClass());

	UClass* SpawnManagerClass = APRSpawnManager::StaticClass();
	TestNotNull(TEXT("Spawn manager accepts director-approved spawn requests"),
		SpawnManagerClass->FindFunctionByName(TEXT("ExecuteEncounterSpawnRequest")));
	TestNotNull(TEXT("Spawn manager exposes its registered encounter region"),
		SpawnManagerClass->FindFunctionByName(TEXT("GetEncounterRegionId")));

	UClass* SpawnPointClass = APREnemySpawnPoint::StaticClass();
	TestNotNull(TEXT("Spawn point has a configured encounter group"),
		SpawnPointClass->FindPropertyByName(TEXT("SpawnGroupId")));
	TestNotNull(TEXT("Spawn point can filter objective nodes"),
		SpawnPointClass->FindPropertyByName(TEXT("AllowedObjectiveNodeIds")));

	UClass* RiftGameStateClass = APRRiftGameState::StaticClass();
	TestNotNull(TEXT("Rift GameState replicates encounter director summary"),
		RiftGameStateClass->FindPropertyByName(TEXT("EncounterDirectorSnapshot")));

	UClass* SettingsClass = UPRProjectSettings::StaticClass();
	TestNotNull(TEXT("Settings expose threat budget defaults"),
		SettingsClass->FindPropertyByName(TEXT("EncounterBaseThreatBudget")));
	TestNotNull(TEXT("Settings expose safe spawn distance"),
		SettingsClass->FindPropertyByName(TEXT("EncounterMinimumPlayerDistance")));
	TestNotNull(TEXT("Settings expose bounded safe-spawn candidate attempts"),
		SettingsClass->FindPropertyByName(TEXT("EncounterMaximumCandidateAttempts")));

	return true;
}

#endif
