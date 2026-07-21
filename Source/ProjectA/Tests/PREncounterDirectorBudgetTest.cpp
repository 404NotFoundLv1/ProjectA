#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PREncounterDirectorComponent.h"
#include "Core/PREncounterDirectorTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREncounterDirectorBudgetTest,
	"ProjectRift.Rift.EncounterDirector.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREncounterDirectorBudgetTest::RunTest(const FString& Parameters)
{
	UPREncounterDirectorComponent* Director = NewObject<UPREncounterDirectorComponent>();
	TestNotNull(TEXT("Director instance is available for pure budget evaluation"), Director);
	if (!Director) return false;

	FPREncounterScalingSnapshot Solo;
	Solo.FrozenPlayerCount = 1;
	Solo.RiskEnemyBudgetMultiplier = 1.0f;
	TestEqual(TEXT("Solo baseline threat is four"), Director->CalculateTargetThreatBudget(Solo), 4.0f);

	FPREncounterScalingSnapshot FourPlayer = Solo;
	FourPlayer.FrozenPlayerCount = 4;
	FourPlayer.RiskEnemyBudgetMultiplier = 1.0f;
	TestEqual(TEXT("Four players receive the frozen-party budget"), Director->CalculateTargetThreatBudget(FourPlayer), 10.0f);

	FourPlayer.RiskEnemyBudgetMultiplier = 1.6f;
	TestEqual(TEXT("Risk-scaled four-player threat clamps at the configured ceiling"), Director->CalculateTargetThreatBudget(FourPlayer), 16.0f);

	const FVector NavigationLocation(120.0f, -40.0f, 15.0f);
	TestEqual(
		TEXT("Navigation ground locations are lifted to the enemy capsule center for collision checks"),
		ProjectRiftEncounterSpawn::GetCapsuleCenterForNavigationLocation(NavigationLocation, 90.0f),
		NavigationLocation + FVector(0.0f, 0.0f, 90.0f));
	return true;
}

#endif
