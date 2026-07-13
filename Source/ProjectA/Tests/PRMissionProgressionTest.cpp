#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Progression/PRMissionProgressionDataAsset.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMissionProgressionPolicyTest,
	"ProjectRift.Progression.MissionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMissionProgressionPolicyTest::RunTest(const FString& Parameters)
{
	UPRMissionProgressionDataAsset* Mission = NewObject<UPRMissionProgressionDataAsset>(GetTransientPackage());
	Mission->MissionId = TEXT("Mission.Rift.Test.Hold");
	Mission->MissionMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ProjectRift/Maps/L_Rift_Test.L_Rift_Test")));
	Mission->ChapterId = TEXT("Chapter.Prologue");
	Mission->CompletionStoryNodeId = TEXT("Story.Prologue.RiftTestHold");
	Mission->bStarterMission = true;

	FString Diagnostic;
	TestTrue(TEXT("Starter mission contract is valid without prerequisites"), Mission->IsContractValid(&Diagnostic));

	FPRProfileStoryProgress EmptyStory;
	TestTrue(TEXT("Starter mission accepts an empty story"), Mission->IsEligible(EmptyStory));

	Mission->bStarterMission = false;
	Mission->RequiredCompletedStoryNodeIds = { TEXT("Story.Prologue.Intro") };
	TestFalse(TEXT("Locked mission rejects a player missing prerequisites"), Mission->IsEligible(EmptyStory));

	FPRProfileStoryProgress EligibleStory;
	EligibleStory.UnlockedChapterIds = { TEXT("Chapter.Prologue") };
	EligibleStory.CompletedStoryNodeIds = { TEXT("Story.Prologue.Intro") };
	TestTrue(TEXT("Mission accepts a player with chapter and prerequisites"), Mission->IsEligible(EligibleStory));

	Mission->UnlockedChapterIdsOnCompletion = { TEXT("Chapter.Prologue"), TEXT("Chapter.One") };
	Mission->NextChapterId = TEXT("Chapter.One");
	TestTrue(TEXT("Eligible completion mutates story once"), Mission->ApplyCompletion(EligibleStory));
	TestTrue(TEXT("Completion node is recorded"), EligibleStory.CompletedStoryNodeIds.Contains(Mission->CompletionStoryNodeId));
	TestTrue(TEXT("Unlocked chapter is added"), EligibleStory.UnlockedChapterIds.Contains(TEXT("Chapter.One")));
	TestEqual(TEXT("Current chapter advances"), EligibleStory.CurrentChapterId, FName(TEXT("Chapter.One")));
	TestFalse(TEXT("Repeating completion is a monotonic no-op"), Mission->ApplyCompletion(EligibleStory));

	Mission->MissionId = NAME_None;
	TestFalse(TEXT("Mission contract rejects an empty mission id"), Mission->IsContractValid(&Diagnostic));

	return true;
}

#endif
