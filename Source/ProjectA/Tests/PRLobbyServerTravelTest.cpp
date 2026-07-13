#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRLobbyServerTravelTest, "ProjectRift.Lobby.ServerTravel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRLobbyServerTravelTest::RunTest(const FString& Parameters)
{
	const APRShipLobbyGameMode* GameModeDefaults = GetDefault<APRShipLobbyGameMode>();
	TestEqual(
		TEXT("Lobby travel target defaults to the rift test map"),
		GameModeDefaults->GetRiftTestMapPath(),
		FString(TEXT("/Game/ProjectRift/Maps/L_Rift_Test")));
	TestEqual(
		TEXT("Lobby travel URL keeps listen server open and carries the authoritative mission"),
		GameModeDefaults->BuildRiftTravelURL(),
		FString(TEXT("/Game/ProjectRift/Maps/L_Rift_Test?listen?MissionId=Mission.Rift.Test.Hold")));

	APRShipLobbyGameMode* LobbyGameMode = NewObject<APRShipLobbyGameMode>();
	TArray<APlayerState*> PlayerStates;
	TestFalse(TEXT("An empty lobby cannot start rift travel"), LobbyGameMode->ArePlayerStatesReadyForTravel(PlayerStates));

	APRPlayerState* ReadyPlayer = NewObject<APRPlayerState>();
	ReadyPlayer->SetReady(true);
	PlayerStates.Add(ReadyPlayer);
	TestFalse(TEXT("A ready player without a bound profile cannot start rift travel"), LobbyGameMode->ArePlayerStatesReadyForTravel(PlayerStates));

	FPRMultiplayerProfileProjection Projection;
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Ready Profile");
	FString BindDiagnostic;
	TestTrue(TEXT("A valid multiplayer projection binds to PlayerState"), ReadyPlayer->BindMultiplayerProfile(Projection, BindDiagnostic));
	ReadyPlayer->SetReady(true);
	TestTrue(TEXT("A lobby with every bound player ready can start rift travel"), LobbyGameMode->ArePlayerStatesReadyForTravel(PlayerStates));

	APRPlayerState* WaitingPlayer = NewObject<APRPlayerState>();
	WaitingPlayer->SetReady(false);
	PlayerStates.Add(WaitingPlayer);
	TestFalse(TEXT("A lobby with any waiting player cannot start rift travel"), LobbyGameMode->ArePlayerStatesReadyForTravel(PlayerStates));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	UFunction* ServerStartRiftMissionFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerStartRiftMission"));
	TestNotNull(TEXT("APRPlayerController exposes ServerStartRiftMission RPC"), ServerStartRiftMissionFunction);
	TestTrue(
		TEXT("ServerStartRiftMission is a server RPC"),
		ServerStartRiftMissionFunction && ServerStartRiftMissionFunction->HasAnyFunctionFlags(FUNC_Net | FUNC_NetServer));
	TestNotNull(TEXT("APRPlayerController exposes local rift start command"), PlayerControllerClass->FindFunctionByName(TEXT("StartRiftMission")));

	return true;
}

#endif
