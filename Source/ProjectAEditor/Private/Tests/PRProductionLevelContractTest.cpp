#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Production/PRProductionLevelValidator.h"
#include "Production/PRProductionPolicyLoader.h"

namespace
{
bool HasIssue(const TArray<FPRProductionValidationIssue>& Issues, const FString& RuleId)
{
	return Issues.ContainsByPredicate([&RuleId](const FPRProductionValidationIssue& Issue)
	{
		return Issue.RuleId == RuleId && Issue.Severity == EPRProductionValidationSeverity::Error;
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProductionLevelContractTest,
	"ProjectRift.Production.LevelContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProductionLevelContractTest::RunTest(const FString& Parameters)
{
	FPRProductionPolicy Policy;
	TArray<FPRProductionValidationIssue> Issues;
	TestTrue(TEXT("Production policy loads before map validation"), FPRProductionPolicyLoader::LoadProjectPolicy(Policy, Issues));
	Issues.Reset();
	FPRProductionLevelValidator::Validate(Policy, Issues);
	TestFalse(TEXT("Every declared production map loads"), HasIssue(Issues, TEXT("PRP-LVL-000")));
	TestFalse(TEXT("Fixture has required spawn groups"), HasIssue(Issues, TEXT("PRP-LVL-001")));
	TestFalse(TEXT("Fixture has required objective sockets"), HasIssue(Issues, TEXT("PRP-LVL-002")));
	TestFalse(TEXT("Fixture has extraction socket"), HasIssue(Issues, TEXT("PRP-LVL-003")));
	TestFalse(TEXT("Fixture has Boss arena"), HasIssue(Issues, TEXT("PRP-LVL-004")));
	TestFalse(TEXT("Fixture has streaming partition"), HasIssue(Issues, TEXT("PRP-LVL-005")));
	return true;
}

#endif
