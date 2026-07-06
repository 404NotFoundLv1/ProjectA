#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/PRGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRSessionInterfaceTest, "ProjectRift.Session.Interface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSessionInterfaceTest::RunTest(const FString& Parameters)
{
	UClass* GameInstanceClass = UPRGameInstance::StaticClass();

	TestNotNull(TEXT("CreateSession entry point exists"), GameInstanceClass->FindFunctionByName(TEXT("CreateSession")));
	TestNotNull(TEXT("FindSessions entry point exists"), GameInstanceClass->FindFunctionByName(TEXT("FindSessions")));
	TestNotNull(TEXT("JoinSession entry point exists"), GameInstanceClass->FindFunctionByName(TEXT("JoinSession")));
	TestNotNull(TEXT("DestroySession entry point exists"), GameInstanceClass->FindFunctionByName(TEXT("DestroySession")));
	TestNotNull(TEXT("StartSession entry point exists"), GameInstanceClass->FindFunctionByName(TEXT("StartSession")));

	UPRGameInstance* GameInstance = NewObject<UPRGameInstance>();
	TestEqual(
		TEXT("Session interface starts idle"),
		GameInstance->GetSessionInterfaceState(),
		EPRSessionInterfaceState::Idle);
	TestTrue(TEXT("CreateSession placeholder succeeds"), GameInstance->CreateSession());
	TestEqual(
		TEXT("CreateSession records in-session placeholder state"),
		GameInstance->GetSessionInterfaceState(),
		EPRSessionInterfaceState::InSession);
	TestTrue(TEXT("StartSession placeholder succeeds after create"), GameInstance->StartSession());
	TestTrue(TEXT("DestroySession placeholder succeeds"), GameInstance->DestroySession());
	TestEqual(
		TEXT("DestroySession returns to idle"),
		GameInstance->GetSessionInterfaceState(),
		EPRSessionInterfaceState::Idle);

	TestTrue(TEXT("FindSessions placeholder succeeds"), GameInstance->FindSessions());
	TestEqual(
		TEXT("FindSessions leaves an empty placeholder result cache"),
		GameInstance->GetCachedSessionSearchResults().Num(),
		0);

	TestFalse(TEXT("JoinSession rejects empty session id"), GameInstance->JoinSession(TEXT("")));
	TestEqual(
		TEXT("JoinSession records a useful error for empty id"),
		GameInstance->GetLastSessionError(),
		FString(TEXT("SessionId is empty.")));

	return true;
}

#endif
