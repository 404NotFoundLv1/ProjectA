#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/PRMissionSelectionWidget.h"
#include "Player/PRPlayerController.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRMissionSelectionUIContractTest, "ProjectRift.MissionContracts.UI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPRMissionSelectionUIContractTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Mission selection row is reflected"), FPRMissionSelectionRow::StaticStruct());
	TestNotNull(TEXT("Mission selection widget is reflected"), UPRMissionSelectionWidget::StaticClass());
	UClass* WidgetClass = UPRMissionSelectionWidget::StaticClass();
	TestNotNull(TEXT("Widget exposes refresh"), WidgetClass->FindFunctionByName(TEXT("RefreshMissionDisplay")));
	TestNotNull(TEXT("Widget exposes focus next"), WidgetClass->FindFunctionByName(TEXT("FocusNext")));
	TestNotNull(TEXT("Widget exposes focus previous"), WidgetClass->FindFunctionByName(TEXT("FocusPrevious")));
	TestNotNull(TEXT("Widget exposes select request"), WidgetClass->FindFunctionByName(TEXT("RequestFocusedSelection")));
	TestNotNull(TEXT("Controller exposes ContractId-only mission request"), APRPlayerController::StaticClass()->FindFunctionByName(TEXT("SelectMissionContract")));
	return true;
}
#endif
