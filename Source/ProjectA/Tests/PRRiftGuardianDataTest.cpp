#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Bosses/PRBossEncounterController.h"
#include "Bosses/PRBossObjectiveActor.h"
#include "Core/PRAssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Ship/PRShipRepairDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRiftGuardianDataTest,
	"ProjectRift.Rift.Guardian.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRiftGuardianDataTest::RunTest(const FString& Parameters)
{
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	UPRBossDefinitionDataAsset* Boss = AssetManager ? AssetManager->LoadBossDefinitionSync(TEXT("Boss.RiftGuardian")) : nullptr;
	TestNotNull(TEXT("Rift Guardian is a registered Boss definition"), Boss);
	if (Boss)
	{
		TestEqual(TEXT("Rift Guardian has exactly two authored phases"), Boss->Phases.Num(), 2);
		TestTrue(TEXT("Rift Guardian has the required four active patterns"), Boss->AbilityPatterns.Num() >= 4);
		TestEqual(TEXT("Rift Guardian scales solo health from its authored baseline"), Boss->Attributes.MaxHealth, 1600.0f);
		TestEqual(TEXT("Rift Guardian four-player health scalar is authored"), Boss->HealthScaling.FourPlayers, 2.7f);
		TestEqual(TEXT("Rift Guardian enters enrage at 35 percent health"), Boss->Phases[1].StartHealthPercent, 0.35f);
		TestTrue(TEXT("Rift Guardian enrage accelerates scheduling"), Boss->Phases[1].CooldownMultiplier < 1.0f);
	}

	UPRMissionProgressionDataAsset* Mission = AssetManager ? AssetManager->LoadMissionSync(TEXT("Mission.Rift.Guardian")) : nullptr;
	TestNotNull(TEXT("Rift Guardian mission is a registered contract"), Mission);
	if (Mission)
	{
		TestTrue(TEXT("Guardian contract has a graph-backed prelude and Boss objective"), Mission->ObjectiveGraph.Nodes.Num() == 2);
		TestEqual(TEXT("Guardian prelude is the authored 20-second Hold objective"), Mission->ObjectiveGraph.Nodes[0].ObjectiveType, EPRObjectiveType::Hold);
		TestTrue(TEXT("Guardian prelude drives the encounter director"), Mission->ObjectiveGraph.Nodes[0].bDrivesEnemySpawning);
		TestEqual(TEXT("Guardian completion writes its dedicated story node"), Mission->CompletionStoryNodeId, FName(TEXT("Story.Prologue.RiftGuardianDefeated")));
	}

	UPRShipRepairDataAsset* Repair = AssetManager ? AssetManager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage2")) : nullptr;
	TestNotNull(TEXT("Guardian unlocks the second engine repair contract"), Repair);
	if (Repair)
	{
		TestEqual(TEXT("Second engine repair targets level two"), Repair->TargetLevel, 2);
		TestTrue(TEXT("Second engine repair requires the Guardian story completion"), Repair->RequiredCompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftGuardianDefeated")));
	}

	UWorld* GuardianWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/ProjectRift/Maps/L_Rift_Guardian.L_Rift_Guardian"));
	TestNotNull(TEXT("Dedicated Guardian graybox map loads"), GuardianWorld);
	int32 EncounterControllerCount = 0;
	int32 GuardianObjectiveCount = 0;
	if (GuardianWorld && GuardianWorld->PersistentLevel)
	{
		for (AActor* Actor : GuardianWorld->PersistentLevel->Actors)
		{
			EncounterControllerCount += Actor && Actor->IsA<APRBossEncounterController>() ? 1 : 0;
			GuardianObjectiveCount += Actor && Actor->IsA<APRBossObjectiveActor>() ? 1 : 0;
		}
	}
	TestEqual(TEXT("Guardian map places one Boss encounter controller"), EncounterControllerCount, 1);
	TestEqual(TEXT("Guardian map places one Boss graph objective"), GuardianObjectiveCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRiftGuardianValidationTest,
	"ProjectRift.Rift.Guardian.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRiftGuardianValidationTest::RunTest(const FString& Parameters)
{
	UPRBossDefinitionDataAsset* Definition = NewObject<UPRBossDefinitionDataAsset>();
	Definition->BossId = TEXT("Boss.Validation.Guardian");
	FPRBossPhaseDefinition PhaseOne;
	PhaseOne.PhaseId = TEXT("Phase.One");
	PhaseOne.StartHealthPercent = 1.0f;
	FPRBossPhaseDefinition PhaseTwo;
	PhaseTwo.PhaseId = TEXT("Phase.Two");
	PhaseTwo.StartHealthPercent = 0.35f;
	Definition->Phases = { PhaseOne, PhaseTwo };
	FPRBossAbilityPatternDefinition Pattern;
	Pattern.PatternId = TEXT("Pattern.Line");
	Pattern.Weight = 1.0f;
	Pattern.BaseDamage = 10.0f;
	Pattern.Telegraph.Shape = EPRBossTelegraphShape::Line;
	Definition->AbilityPatterns = { Pattern };

	FString Diagnostic;
	TestFalse(TEXT("Damaging line telegraphs reject missing dimensions"), Definition->ValidateDefinition(Diagnostic));

	Definition->AbilityPatterns[0].Telegraph.Length = 400.0f;
	Definition->AbilityPatterns[0].Telegraph.Width = 100.0f;
	Definition->AbilityPatterns[0].SummonCount = 1;
	Definition->AbilityPatterns[0].SummonDefinitionId = TEXT("Enemy.Regular.Crawler");
	Definition->AbilityPatterns[0].AdditionalSummonsPerPlayer = 1;
	Definition->AbilityPatterns[0].MaxSummons = 0;
	TestFalse(TEXT("Scaled summons reject a zero cap"), Definition->ValidateDefinition(Diagnostic));

	Definition->AbilityPatterns[0].MaxSummons = 4;
	Definition->Phases[1].CooldownMultiplier = 0.0f;
	TestFalse(TEXT("Boss phases reject a non-positive cooldown multiplier"), Definition->ValidateDefinition(Diagnostic));
	return true;
}

#endif
