#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Production/PRProductionPolicyLoader.h"
#include "Production/PRProductionValidationService.h"

namespace
{
bool HasIssue(const TArray<FPRProductionValidationIssue>& Issues, const FString& RuleId)
{
	return Issues.ContainsByPredicate([&RuleId](const FPRProductionValidationIssue& Issue)
	{
		return Issue.RuleId == RuleId;
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProductionPolicyTest,
	"ProjectRift.Production.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProductionPolicyTest::RunTest(const FString& Parameters)
{
	const FString ValidPolicy = TEXT(R"JSON({
		"SchemaVersion": 1,
		"ProjectVersion": "0.9.0",
		"ScanRoots": ["/Game/ProjectRift"],
		"AllowedExternalRoots": ["/Engine", "/Game/Characters", "/Game/ThirdPerson"],
		"AssetRules": [{"ClassName":"Texture2D","Prefix":"T_","MaxDimension":4096}],
		"LevelContracts": []
	})JSON");

	FPRProductionPolicy Policy;
	TArray<FPRProductionValidationIssue> Issues;
	TestTrue(TEXT("Strict production policy parses"), FPRProductionPolicyLoader::LoadPolicyFromJson(ValidPolicy, Policy, Issues));
	TestEqual(TEXT("Policy keeps its only scan root"), Policy.ScanRoots.Num(), 1);
	TestFalse(TEXT("Policy has no parse errors"), HasIssue(Issues, TEXT("PRP-POL-001")));

	const FString UnknownPermissionRegistry = TEXT(R"JSON({
		"SchemaVersion": 1,
		"Records": [{
			"AssetPath":"/Game/ProjectRift/VFX/NS_Unreviewed",
			"SourceKind":"ProjectOwned",
			"CommercialUse":"Unknown",
			"Modification":"Allowed",
			"Attribution":"NotRequired",
			"LicenseDocumentPath":"Docs/License.txt"
		}]
	})JSON");
	FPRLicenseRegistry Registry;
	Issues.Reset();
	TestFalse(TEXT("Unknown commercial permission fails closed"), FPRProductionPolicyLoader::LoadLicenseRegistryFromJson(UnknownPermissionRegistry, Registry, Issues));
	TestTrue(TEXT("Unknown permission produces a stable license issue"), HasIssue(Issues, TEXT("PRP-LIC-002")));

	const FString CoverageRegistry = TEXT(R"JSON({
		"SchemaVersion": 1,
		"Records": [
			{"AssetPath":"/Game/ProjectRift/VFX","bFolderCoverage":true,"SourceKind":"ProjectOwned","CommercialUse":"Allowed","Modification":"Allowed","Attribution":"NotRequired","LicenseDocumentPath":"Docs/License.txt"},
			{"AssetPath":"/Game/ProjectRift/VFX/NS_Validated","bFolderCoverage":false,"SourceKind":"EpicProvided","CommercialUse":"Allowed","Modification":"Allowed","Attribution":"Required","AttributionText":"Epic Games","LicenseDocumentPath":"Docs/Epic.txt"}
		]
	})JSON");
	Issues.Reset();
	TestTrue(TEXT("Exact and folder coverage parse"), FPRProductionPolicyLoader::LoadLicenseRegistryFromJson(CoverageRegistry, Registry, Issues));
	const FPRLicenseRecord* Coverage = Registry.FindCoverage(TEXT("/Game/ProjectRift/VFX/NS_Validated"));
	TestNotNull(TEXT("Exact asset has license coverage"), Coverage);
	if (Coverage)
	{
		TestFalse(TEXT("Exact asset overrides folder coverage"), Coverage->bFolderCoverage);
	}

	FPRLicenseRegistry ProjectRegistry;
	Issues.Reset();
	TestTrue(TEXT("Project registry reloads for evidence validation"), FPRProductionPolicyLoader::LoadProjectLicenseRegistry(ProjectRegistry, Issues));
	Issues = FPRProductionValidationService::ValidateLicenseEvidence(ProjectRegistry);
	TestFalse(TEXT("Reviewed registry evidence paths resolve inside ProjectA"), HasIssue(Issues, TEXT("PRP-LIC-003")));
	return true;
}

#endif
