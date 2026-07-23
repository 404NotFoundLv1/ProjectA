#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Production/PRProductionValidationReportWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProductionReportTest,
	"ProjectRift.Production.Report",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProductionReportTest::RunTest(const FString& Parameters)
{
	FPRProductionValidationReport Report;
	Report.ProjectVersion = TEXT("0.9.0");
	Report.ScannedAssetCount = 3;
	Report.WarningCount = 1;
	FPRProductionValidationIssue& Issue = Report.Issues.AddDefaulted_GetRef();
	Issue.RuleId = TEXT("PRP-UNU-001");
	Issue.Severity = EPRProductionValidationSeverity::Warning;
	Issue.AssetPath = TEXT("/Game/ProjectRift/UI/T_Unused");
	Issue.Message = TEXT("Unused fixture asset.");
	Issue.Remediation = TEXT("Review manually.");

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectRiftProductionValidation/TestReport"));
	FPRProductionValidationReportPaths Paths;
	TestTrue(TEXT("Production report writes both formats"), FPRProductionValidationReportWriter::Write(Report, Directory, Paths));
	TestTrue(TEXT("JSON report exists"), FPaths::FileExists(Paths.JsonPath));
	TestTrue(TEXT("Markdown report exists"), FPaths::FileExists(Paths.MarkdownPath));
	FString Markdown;
	TestTrue(TEXT("Markdown report loads"), FFileHelper::LoadFileToString(Markdown, *Paths.MarkdownPath));
	TestTrue(TEXT("Markdown contains the stable rule identifier"), Markdown.Contains(TEXT("PRP-UNU-001")));
	return true;
}

#endif
