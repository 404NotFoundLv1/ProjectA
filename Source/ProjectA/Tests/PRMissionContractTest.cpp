#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Progression/PRMissionContractTypes.h"
#include "Progression/PRMissionProgressionDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMissionContractDefinitionTest,
	"ProjectRift.MissionContracts.Definition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMissionContractDefinitionTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Mission reward preview is reflected"), FPRMissionRewardPreview::StaticStruct());
	TestNotNull(TEXT("Mission contract is reflected"), FPRMissionContract::StaticStruct());
	TestNotNull(TEXT("Mission definition is reflected"), FPRMissionDefinition::StaticStruct());
	TestNotNull(TEXT("Mission travel context is reflected"), FPRMissionTravelContext::StaticStruct());

	UPRMissionProgressionDataAsset* Mission = NewObject<UPRMissionProgressionDataAsset>(GetTransientPackage());
	Mission->MissionId = TEXT("Mission.Contract.Test");
	Mission->MissionMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ProjectRift/Maps/L_Rift_Test.L_Rift_Test")));
	Mission->ChapterId = TEXT("Chapter.Prologue");
	Mission->CompletionStoryNodeId = TEXT("Story.Contract.Test");
	Mission->bStarterMission = true;
	Mission->Contract.ContractVersion = 1;
	Mission->Contract.BiomeId = TEXT("Biome.Test");
	Mission->Contract.DifficultyId = TEXT("Difficulty.Standard");
	Mission->Contract.ModifierIds = { TEXT("Modifier.B"), TEXT("Modifier.A") };
	Mission->Contract.RewardPreview.RewardTypeIds = { TEXT("Reward.Material"), TEXT("Reward.Equipment") };
	Mission->Contract.RewardPreview.MinimumBudget = 2;
	Mission->Contract.RewardPreview.MaximumBudget = 4;
	Mission->Contract.RewardPreview.MinimumRarity = 0;
	Mission->Contract.RewardPreview.MaximumRarity = 2;

	FString Diagnostic;
	TestTrue(TEXT("A complete authored mission contract validates"), Mission->ValidateMissionContract(&Diagnostic));
	const FPRMissionDefinition First = Mission->BuildMissionDefinition(1701, &Diagnostic);
	const FPRMissionDefinition Repeat = Mission->BuildMissionDefinition(1701, &Diagnostic);
	TestTrue(TEXT("A non-zero seed resolves a valid mission definition"), First.IsValid());
	TestEqual(TEXT("Repeated seed preserves the deterministic signature"), First.DeterministicSignature, Repeat.DeterministicSignature);
	TestEqual(TEXT("Definition normalizes modifier order"), First.ModifierIds, TArray<FName>({ TEXT("Modifier.A"), TEXT("Modifier.B") }));

	const FPRMissionDefinition ChangedSeed = Mission->BuildMissionDefinition(1702, &Diagnostic);
	TestNotEqual(TEXT("Changing seed changes deterministic signature"), First.DeterministicSignature, ChangedSeed.DeterministicSignature);
	TestFalse(TEXT("Zero seed is rejected"), Mission->BuildMissionDefinition(0, &Diagnostic).IsValid());

	Mission->Contract.ModifierIds.Add(TEXT("Modifier.A"));
	TestFalse(TEXT("Duplicate modifier ids are rejected"), Mission->ValidateMissionContract(&Diagnostic));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMissionTravelContextTest,
	"ProjectRift.MissionContracts.TravelContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMissionTravelContextTest::RunTest(const FString& Parameters)
{
	FPRMissionTravelContext Context;
	Context.ContractId = TEXT("Mission.Contract.Test");
	Context.ContractVersion = 3;
	Context.Seed = 1701;
	const FString Options = Context.ToTravelOptions();
	TestTrue(TEXT("Compact travel context is valid"), Context.IsValid());
	TestTrue(TEXT("Travel context contains ContractId"), Options.Contains(TEXT("ContractId=Mission.Contract.Test")));
	TestTrue(TEXT("Travel context contains contract version"), Options.Contains(TEXT("ContractVersion=3")));
	TestTrue(TEXT("Travel context contains seed"), Options.Contains(TEXT("Seed=1701")));

	FPRMissionTravelContext Parsed;
	FString Diagnostic;
	TestTrue(TEXT("Compact travel context round-trips"), FPRMissionTravelContext::ParseOptions(Options, Parsed, Diagnostic));
	TestEqual(TEXT("Parsed contract id is preserved"), Parsed.ContractId, Context.ContractId);
	TestEqual(TEXT("Parsed version is preserved"), Parsed.ContractVersion, Context.ContractVersion);
	TestEqual(TEXT("Parsed seed is preserved"), Parsed.Seed, Context.Seed);
	TestFalse(TEXT("Zero-seed travel context is rejected"), FPRMissionTravelContext::ParseOptions(TEXT("ContractId=Mission.Contract.Test?ContractVersion=3?Seed=0"), Parsed, Diagnostic));
	TestTrue(TEXT("Legacy MissionId remains accepted"), FPRMissionTravelContext::ParseOptions(TEXT("MissionId=Mission.Contract.Test"), Parsed, Diagnostic));
	TestEqual(TEXT("Legacy MissionId becomes contract id"), Parsed.ContractId, FName(TEXT("Mission.Contract.Test")));

	return true;
}

#endif
