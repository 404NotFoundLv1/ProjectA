#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREnemyEcologyDataTest,
	"ProjectRift.Rift.EnemyEcology.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREnemyEcologyDataTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Enemy primary asset IDs are canonical"),
		UPRAssetManager::MakeEnemyPrimaryAssetId(TEXT("Enemy.Regular.Crawler")).ToString(),
		FString(TEXT("ProjectRiftEnemy:Enemy.Regular.Crawler")));
	TestEqual(
		TEXT("Enemy roster primary asset IDs are canonical"),
		UPRAssetManager::MakeEnemyRosterPrimaryAssetId(TEXT("Roster.RiftTest")).ToString(),
		FString(TEXT("ProjectRiftEnemyRoster:Roster.RiftTest")));
	TestFalse(TEXT("Empty enemy IDs are invalid"), UPRAssetManager::MakeEnemyPrimaryAssetId(NAME_None).IsValid());
	TestFalse(TEXT("Empty enemy roster IDs are invalid"), UPRAssetManager::MakeEnemyRosterPrimaryAssetId(NAME_None).IsValid());
	return true;
}

#endif
