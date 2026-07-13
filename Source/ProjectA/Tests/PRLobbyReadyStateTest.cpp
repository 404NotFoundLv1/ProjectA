#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRLobbyReadyStateTest, "ProjectRift.Lobby.ReadyState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRLobbyReadyStateTest::RunTest(const FString& Parameters)
{
	UClass* PlayerStateClass = APRPlayerState::StaticClass();
	const APRPlayerState* PlayerStateDefaults = GetDefault<APRPlayerState>();

	const FBoolProperty* ReadyProperty = FindFProperty<FBoolProperty>(PlayerStateClass, TEXT("bIsReady"));
	const FNameProperty* RoleProperty = FindFProperty<FNameProperty>(PlayerStateClass, TEXT("SelectedRoleModule"));
	const FStrProperty* DisplayNameProperty = FindFProperty<FStrProperty>(PlayerStateClass, TEXT("PlayerDisplayName"));

	TestNotNull(TEXT("APRPlayerState owns replicated ready state"), ReadyProperty);
	TestTrue(TEXT("Ready state is replicated"), ReadyProperty && ReadyProperty->HasAnyPropertyFlags(CPF_Net));
	TestNotNull(TEXT("APRPlayerState owns selected role module"), RoleProperty);
	TestTrue(TEXT("Selected role module is replicated"), RoleProperty && RoleProperty->HasAnyPropertyFlags(CPF_Net));
	TestNotNull(TEXT("APRPlayerState owns player display name"), DisplayNameProperty);
	TestTrue(TEXT("Player display name is replicated"), DisplayNameProperty && DisplayNameProperty->HasAnyPropertyFlags(CPF_Net));

	TestFalse(TEXT("Players default to not ready"), PlayerStateDefaults->IsReady());
	TestEqual(TEXT("Players default to no selected role module"), PlayerStateDefaults->GetSelectedRoleModule(), NAME_None);
	TestFalse(TEXT("Players have a fallback lobby display name"), PlayerStateDefaults->GetPlayerDisplayName().IsEmpty());

	APRPlayerState* MutablePlayerState = NewObject<APRPlayerState>();
	FPRMultiplayerProfileProjection Projection;
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Test Pilot");
	FString BindingDiagnostic;
	TestTrue(TEXT("Ready-state test profile binds first"), MutablePlayerState->BindMultiplayerProfile(Projection, BindingDiagnostic));
	MutablePlayerState->SetReady(true);
	MutablePlayerState->SetSelectedRoleModule(TEXT("Role.Engineer"));
	MutablePlayerState->SetPlayerDisplayName(TEXT("Test Pilot"));

	TestTrue(TEXT("Server-side ready setter updates state"), MutablePlayerState->IsReady());
	TestEqual(TEXT("Server-side role setter updates state"), MutablePlayerState->GetSelectedRoleModule(), FName(TEXT("Role.Engineer")));
	TestEqual(TEXT("Server-side display name setter updates state"), MutablePlayerState->GetPlayerDisplayName(), FString(TEXT("Test Pilot")));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	const FClassProperty* GASDebugWidgetClassProperty = FindFProperty<FClassProperty>(PlayerControllerClass, TEXT("GASDebugWidgetClass"));
	TestNotNull(TEXT("APRPlayerController exposes GAS debug widget class"), GASDebugWidgetClassProperty);

	UFunction* ServerSetReadyFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerSetReady"));
	TestNotNull(TEXT("APRPlayerController exposes ServerSetReady RPC"), ServerSetReadyFunction);
	TestTrue(TEXT("ServerSetReady is a server RPC"), ServerSetReadyFunction && ServerSetReadyFunction->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer));
	TestNotNull(TEXT("APRPlayerController exposes local ready toggle"), PlayerControllerClass->FindFunctionByName(TEXT("ToggleReady")));

	UFunction* ServerSetSelectedRoleModuleFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerSetSelectedRoleModule"));
	TestNotNull(TEXT("APRPlayerController exposes ServerSetSelectedRoleModule RPC"), ServerSetSelectedRoleModuleFunction);
	TestTrue(TEXT("ServerSetSelectedRoleModule is a server RPC"), ServerSetSelectedRoleModuleFunction && ServerSetSelectedRoleModuleFunction->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer));
	TestNotNull(TEXT("APRPlayerController exposes local assault role selection entry"), PlayerControllerClass->FindFunctionByName(TEXT("SelectAssaultRoleModule")));
	TestNotNull(TEXT("APRPlayerController exposes generic role selection entry"), PlayerControllerClass->FindFunctionByName(TEXT("SelectRoleModule")));

	return true;
}

#endif
