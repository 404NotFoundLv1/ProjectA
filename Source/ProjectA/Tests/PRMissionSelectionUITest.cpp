#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	TestNotNull(TEXT("Controller exposes mission panel toggle"), APRPlayerController::StaticClass()->FindFunctionByName(TEXT("ToggleMissionSelectionPanel")));
	FString ControllerSource;
	const FString ControllerPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ProjectA/Player/PRPlayerController.cpp"));
	TestTrue(TEXT("PlayerController source is readable"), FFileHelper::LoadFileToString(ControllerSource, *ControllerPath));
	TestTrue(TEXT("Lobby mission selection binds M"), ControllerSource.Contains(TEXT("BindKey(EKeys::M, IE_Pressed, this, &APRPlayerController::ToggleMissionSelectionPanel)")));
	return true;
}
#endif
