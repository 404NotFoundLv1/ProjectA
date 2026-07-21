#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Core/PRAssetManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRBossFrameworkDataTest,
	"ProjectRift.Rift.BossFramework.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRBossFrameworkDataTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Boss primary asset IDs are canonical"),
		UPRAssetManager::MakeBossPrimaryAssetId(TEXT("Boss.Framework.Test")).ToString(),
		FString(TEXT("ProjectRiftBoss:Boss.Framework.Test")));
	TestFalse(TEXT("Empty Boss IDs are invalid"), UPRAssetManager::MakeBossPrimaryAssetId(NAME_None).IsValid());

	FPRBossPlayerCountScalar Scalar;
	Scalar.Solo = 1.0f;
	Scalar.TwoPlayers = 1.6f;
	Scalar.ThreePlayers = 2.1f;
	Scalar.FourPlayers = 2.5f;
	TestEqual(TEXT("Boss solo scaling is selected"), Scalar.GetForPlayerCount(1), 1.0f);
	TestEqual(TEXT("Boss four-player scaling is selected"), Scalar.GetForPlayerCount(4), 2.5f);

	UPRBossDefinitionDataAsset* Definition = NewObject<UPRBossDefinitionDataAsset>();
	Definition->BossId = TEXT("Boss.Framework.Test");
	FPRBossPhaseDefinition PhaseOne;
	PhaseOne.PhaseId = TEXT("Phase.One");
	PhaseOne.StartHealthPercent = 1.0f;
	FPRBossPhaseDefinition PhaseTwo;
	PhaseTwo.PhaseId = TEXT("Phase.Two");
	PhaseTwo.StartHealthPercent = 0.55f;
	Definition->Phases = { PhaseOne, PhaseTwo };
	FPRBossAbilityPatternDefinition Pattern;
	Pattern.PatternId = TEXT("Pattern.Targeted");
	Pattern.Weight = 1.0f;
	Pattern.BaseDamage = 10.0f;
	Pattern.Telegraph.Shape = EPRBossTelegraphShape::Targeted;
	Definition->AbilityPatterns.Add(Pattern);
	FString Diagnostic;
	TestTrue(TEXT("Strictly descending Boss phases are valid"), Definition->ValidateDefinition(Diagnostic));

	Definition->Phases[1].StartHealthPercent = 1.0f;
	TestFalse(TEXT("Repeated Boss phase thresholds fail closed"), Definition->ValidateDefinition(Diagnostic));

	Definition->Phases[1].StartHealthPercent = 0.55f;
	Definition->AbilityPatterns[0].Telegraph.Shape = EPRBossTelegraphShape::None;
	TestFalse(TEXT("Damaging boss patterns require a readable telegraph"), Definition->ValidateDefinition(Diagnostic));

	Definition->AbilityPatterns[0].Telegraph.Shape = EPRBossTelegraphShape::Targeted;
	Definition->AbilityPatterns[0].SummonCount = 1;
	TestFalse(TEXT("Summon patterns fail closed without an enemy definition"), Definition->ValidateDefinition(Diagnostic));

	Definition->AbilityPatterns[0].SummonCount = 0;
	Definition->AbilityPatterns[0].ArenaEventId = TEXT("Arena.Event.Missing");
	TestFalse(TEXT("Boss patterns fail closed when an arena event is not declared"), Definition->ValidateDefinition(Diagnostic));
	return true;
}

#endif
