#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRSpawnManager.h"
#include "Enemies/PREnemyBehaviorComponent.h"
#include "Enemies/PREnemyCharacter.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREnemyEcologyContractTest,
	"ProjectRift.Rift.EnemyEcology.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREnemyEcologyContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(
		TEXT("Enemy definitions are reflected primary data assets"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREnemyDefinitionDataAsset")));
	TestNotNull(
		TEXT("Enemy rosters are reflected primary data assets"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREnemyRosterDataAsset")));
	TestNotNull(
		TEXT("Enemy threat component is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREnemyThreatComponent")));
	TestNotNull(
		TEXT("Enemy behavior component is reflected"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREnemyBehaviorComponent")));
	TestNotNull(
		TEXT("Enemy tier enum is reflected"),
		FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPREnemyTier")));
	TestNotNull(
		TEXT("Enemy action definition is reflected"),
		FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREnemyActionDefinition")));

	UClass* EnemyClass = APREnemyCharacter::StaticClass();
	TestNotNull(TEXT("Enemy character exposes replicated definition identity"), EnemyClass->FindPropertyByName(TEXT("EnemyDefinitionId")));
	TestNotNull(TEXT("Enemy character exposes server definition assignment"), EnemyClass->FindFunctionByName(TEXT("SetEnemyDefinition")));
	TestNotNull(TEXT("Enemy character exposes status-immunity query"), EnemyClass->FindFunctionByName(TEXT("IsStatusImmune")));
	const APREnemyCharacter* EnemyDefaults = GetDefault<APREnemyCharacter>();
	TestNotNull(TEXT("Enemy default object exists"), EnemyDefaults);
	if (EnemyDefaults)
	{
		TestFalse(TEXT("Defined enemies do not require actor Tick"), EnemyDefaults->PrimaryActorTick.bCanEverTick);
		const UPREnemyBehaviorComponent* Behavior = EnemyDefaults->GetEnemyBehaviorComponent();
		TestNotNull(TEXT("Enemy default has a shared behavior component"), Behavior);
		if (Behavior)
		{
			TestFalse(TEXT("Shared enemy behavior uses server timers instead of component Tick"), Behavior->PrimaryComponentTick.bCanEverTick);
		}
	}

	UClass* SpawnManagerClass = APRSpawnManager::StaticClass();
	TestNotNull(TEXT("Spawn manager exposes an enemy roster"), SpawnManagerClass->FindPropertyByName(TEXT("EnemyRoster")));
	TestNotNull(TEXT("Spawn manager exposes per-definition alive count"), SpawnManagerClass->FindFunctionByName(TEXT("GetAliveEnemyDefinitionCount")));
	UScriptStruct* EncounterEntry = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PREncounterSpawnEntry"));
	TestNotNull(TEXT("Encounter entry is reflected"), EncounterEntry);
	if (EncounterEntry)
	{
		TestNotNull(TEXT("Encounter entry carries a per-definition cap"), EncounterEntry->FindPropertyByName(TEXT("MaxAlive")));
		TestNotNull(TEXT("Encounter entry carries a risk-tier gate"), EncounterEntry->FindPropertyByName(TEXT("MinimumRiskTier")));
	}
	return true;
}

#endif
