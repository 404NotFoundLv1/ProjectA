#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProductionModuleContractTest,
	"ProjectRift.Production.ModuleContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRProductionModuleContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("ProjectAEditor is registered as an editor-only module"),
		FModuleManager::Get().ModuleExists(TEXT("ProjectAEditor")));
	TestNotNull(
		TEXT("Production level marker is reflected by the runtime module"),
		FindFirstObject<UClass>(TEXT("PRProductionLevelMarker"), EFindFirstObjectOptions::None));
	return true;
}

#endif
