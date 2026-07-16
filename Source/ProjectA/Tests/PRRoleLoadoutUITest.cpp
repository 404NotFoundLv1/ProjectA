#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Player/PRPlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRoleLoadoutUIContractTest,
	"ProjectRift.Roles.Loadout.UI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRoleLoadoutUIContractTest::RunTest(const FString& Parameters)
{
	UClass* RoleWidgetClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRoleLoadoutWidget"));
	TestNotNull(TEXT("Role loadout widget class exists"), RoleWidgetClass);
	TestTrue(
		TEXT("Role loadout widget derives from UUserWidget"),
		RoleWidgetClass && RoleWidgetClass->IsChildOf(UUserWidget::StaticClass()));
	if (RoleWidgetClass)
	{
		TestNotNull(TEXT("Role widget exposes refresh"), RoleWidgetClass->FindFunctionByName(TEXT("RefreshLoadoutDisplay")));
		TestNotNull(TEXT("Role widget exposes apply"), RoleWidgetClass->FindFunctionByName(TEXT("RequestApplyLoadout")));
		TestNotNull(TEXT("Role widget exposes defaults"), RoleWidgetClass->FindFunctionByName(TEXT("RequestRestoreDefaults")));
		TestNotNull(TEXT("Role widget exposes close"), RoleWidgetClass->FindFunctionByName(TEXT("RequestClose")));
	}

	UClass* ControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Player controller exposes role widget class"), FindFProperty<FClassProperty>(ControllerClass, TEXT("RoleLoadoutWidgetClass")));
	TestNotNull(TEXT("Player controller exposes role widget getter"), ControllerClass->FindFunctionByName(TEXT("GetRoleLoadoutWidget")));
	TestNotNull(TEXT("Player controller exposes role panel toggle"), ControllerClass->FindFunctionByName(TEXT("ToggleRoleLoadoutPanel")));
	TestNotNull(TEXT("Player controller exposes role panel show"), ControllerClass->FindFunctionByName(TEXT("ShowRoleLoadoutPanel")));
	TestNotNull(TEXT("Player controller exposes role panel hide"), ControllerClass->FindFunctionByName(TEXT("HideRoleLoadoutPanel")));
	TestNotNull(TEXT("Player controller exposes inventory panel show for role-panel mutual exclusion"), ControllerClass->FindFunctionByName(TEXT("ShowInventory")));
	TestNotNull(TEXT("Player controller exposes ship repair panel show for role-panel mutual exclusion"), ControllerClass->FindFunctionByName(TEXT("ShowShipRepairPanel")));

	UFunction* ApplyRpc = ControllerClass->FindFunctionByName(TEXT("ServerApplyRoleLoadout"));
	TestNotNull(TEXT("Player controller exposes loadout apply RPC"), ApplyRpc);
	TestTrue(TEXT("Loadout apply is a reliable server RPC"), ApplyRpc && ApplyRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetServer | FUNC_NetReliable));
	UFunction* ResultRpc = ControllerClass->FindFunctionByName(TEXT("ClientRoleLoadoutApplyResult"));
	TestNotNull(TEXT("Player controller exposes loadout result RPC"), ResultRpc);
	TestTrue(TEXT("Loadout result is a reliable client RPC"), ResultRpc && ResultRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetClient | FUNC_NetReliable));

	return true;
}

#endif
