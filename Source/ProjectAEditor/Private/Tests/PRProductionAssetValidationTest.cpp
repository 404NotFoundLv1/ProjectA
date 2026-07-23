#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Production/PRProductionPolicyLoader.h"
#include "Production/PRProductionValidationService.h"

namespace
{
bool HasIssue(const TArray<FPRProductionValidationIssue>& Issues, const FString& RuleId, EPRProductionValidationSeverity Severity)
{
	return Issues.ContainsByPredicate([&RuleId, Severity](const FPRProductionValidationIssue& Issue)
	{
		return Issue.RuleId == RuleId && Issue.Severity == Severity;
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProductionAssetValidationTest,
	"ProjectRift.Production.AssetValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProductionAssetValidationTest::RunTest(const FString& Parameters)
{
	FPRProductionPolicy Policy;
	FPRLicenseRegistry Registry;
	TArray<FPRProductionValidationIssue> LoadIssues;
	TestTrue(TEXT("Project production policy loads"), FPRProductionPolicyLoader::LoadProjectPolicy(Policy, LoadIssues));
	TestTrue(TEXT("Project license registry loads"), FPRProductionPolicyLoader::LoadProjectLicenseRegistry(Registry, LoadIssues));

	FPRProductionAssetDescriptor ValidTexture;
	ValidTexture.PackageName = TEXT("/Game/ProjectRift/UI/Icons/T_Validated");
	ValidTexture.AssetName = TEXT("T_Validated");
	ValidTexture.ClassName = TEXT("Texture2D");
	ValidTexture.Width = 1024;
	ValidTexture.Height = 1024;
	TArray<FPRProductionValidationIssue> Issues = FPRProductionValidationService::ValidateDescriptor(ValidTexture, Policy, Registry);
	TestFalse(TEXT("Registered, correctly named texture has no errors"), FPRProductionValidationService::HasErrors(Issues));

	FPRProductionAssetDescriptor BadName = ValidTexture;
	BadName.AssetName = TEXT("Texture_New");
	Issues = FPRProductionValidationService::ValidateDescriptor(BadName, Policy, Registry);
	TestTrue(TEXT("Invalid prefix/name produces PRP-NAM-001"), HasIssue(Issues, TEXT("PRP-NAM-001"), EPRProductionValidationSeverity::Error));

	FPRProductionAssetDescriptor GameplayEffectBlueprint;
	GameplayEffectBlueprint.PackageName = TEXT("/Game/ProjectRift/Abilities/Effects/GE_Damage");
	GameplayEffectBlueprint.AssetName = TEXT("GE_Damage");
	GameplayEffectBlueprint.ClassName = TEXT("Blueprint");
	Issues = FPRProductionValidationService::ValidateDescriptor(GameplayEffectBlueprint, Policy, Registry);
	TestFalse(TEXT("Gameplay Effect Blueprint uses its registered GE_ prefix"), HasIssue(Issues, TEXT("PRP-NAM-001"), EPRProductionValidationSeverity::Error));

	FPRLicenseRegistry EmptyRegistry;
	Issues = FPRProductionValidationService::ValidateDescriptor(ValidTexture, Policy, EmptyRegistry);
	TestTrue(TEXT("Missing license produces PRP-LIC-001"), HasIssue(Issues, TEXT("PRP-LIC-001"), EPRProductionValidationSeverity::Error));

	FPRProductionAssetDescriptor ForbiddenReference = ValidTexture;
	ForbiddenReference.Dependencies.Add(TEXT("/Game/ProjectRift/Developer/Maps/L_ProductionValidation_Test"));
	Issues = FPRProductionValidationService::ValidateDescriptor(ForbiddenReference, Policy, Registry);
	TestTrue(TEXT("Developer hard reference produces PRP-REF-001"), HasIssue(Issues, TEXT("PRP-REF-001"), EPRProductionValidationSeverity::Error));

	FPRProductionAssetDescriptor Unused = ValidTexture;
	Unused.bIsUnused = true;
	Issues = FPRProductionValidationService::ValidateDescriptor(Unused, Policy, Registry);
	TestTrue(TEXT("Unused assets remain warnings"), HasIssue(Issues, TEXT("PRP-UNU-001"), EPRProductionValidationSeverity::Warning));
	return true;
}

#endif
