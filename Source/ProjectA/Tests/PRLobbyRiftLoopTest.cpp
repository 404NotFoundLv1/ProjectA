#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRShipLobbyGameMode.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRLobbyRiftLoopTest, "ProjectRift.Flow.LobbyRiftLobbyLoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FLoopTestPlayer
{
	TObjectPtr<APlayerController> Controller;
	TObjectPtr<APRPlayerState> PlayerState;
	TObjectPtr<ADefaultPawn> Pawn;
};

FPRItemInstance MakeLoopResourceItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}

FLoopTestPlayer SpawnLoopTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
{
	FLoopTestPlayer Result;
	if (!World)
	{
		return Result;
	}

	Result.Controller = World->SpawnActor<APlayerController>();
	Result.PlayerState = Result.Controller ? Result.Controller->GetPlayerState<APRPlayerState>() : nullptr;
	if (!Result.PlayerState)
	{
		Result.PlayerState = World->SpawnActor<APRPlayerState>();
		if (Result.Controller && Result.PlayerState)
		{
			Result.Controller->SetPlayerState(Result.PlayerState);
		}
	}
	Result.Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Loop player controller spawned"), Result.Controller.Get());
	Test.TestNotNull(TEXT("Loop player state spawned"), Result.PlayerState.Get());
	Test.TestNotNull(TEXT("Loop player pawn spawned"), Result.Pawn.Get());

	if (Result.Controller && Result.Pawn)
	{
		Result.Controller->Possess(Result.Pawn);
	}

	if (AGameStateBase* GameState = World->GetGameState())
	{
		if (Result.PlayerState && !GameState->PlayerArray.Contains(Result.PlayerState))
		{
			GameState->AddPlayerState(Result.PlayerState);
		}
	}

	return Result;
}
}

bool FPRLobbyRiftLoopTest::RunTest(const FString& Parameters)
{
	const APRShipLobbyGameMode* LobbyDefaults = GetDefault<APRShipLobbyGameMode>();
	const APRRiftGameMode* RiftDefaults = GetDefault<APRRiftGameMode>();
	TestTrue(TEXT("Ship lobby keeps players during ServerTravel"), LobbyDefaults && LobbyDefaults->bUseSeamlessTravel);
	TestTrue(TEXT("Rift keeps players during return ServerTravel"), RiftDefaults && RiftDefaults->bUseSeamlessTravel);
	TestEqual(
		TEXT("Lobby still travels to the rift test map as a listen server"),
		LobbyDefaults ? LobbyDefaults->BuildRiftTravelURL() : FString(),
		FString(TEXT("/Game/ProjectRift/Maps/L_Rift_Test?listen")));
	TestEqual(
		TEXT("Rift still returns to the ship lobby as a listen server"),
		RiftDefaults ? RiftDefaults->BuildReturnToLobbyTravelURL() : FString(),
		FString(TEXT("/Game/ProjectRift/Maps/L_ShipLobby?listen")));

	APRShipLobbyGameMode* LobbyRules = NewObject<APRShipLobbyGameMode>();
	APRPlayerState* LobbyPlayerOne = NewObject<APRPlayerState>();
	APRPlayerState* LobbyPlayerTwo = NewObject<APRPlayerState>();
	TestNotNull(TEXT("Loop lobby rules object created"), LobbyRules);
	TestNotNull(TEXT("Loop lobby player one state created"), LobbyPlayerOne);
	TestNotNull(TEXT("Loop lobby player two state created"), LobbyPlayerTwo);
	if (!LobbyRules || !LobbyPlayerOne || !LobbyPlayerTwo)
	{
		return false;
	}

	LobbyPlayerOne->SetReady(true);
	LobbyPlayerTwo->SetReady(true);
	LobbyPlayerOne->SetSelectedRoleModule(TEXT("Assault"));
	LobbyPlayerTwo->SetSelectedRoleModule(TEXT("Support"));
	LobbyPlayerOne->AddShipResource(TEXT("CommonChip"), 1);

	TArray<APlayerState*> ReadyLobbyPlayers = { LobbyPlayerOne, LobbyPlayerTwo };
	TestTrue(TEXT("Two ready lobby players can start the rift loop"), LobbyRules->ArePlayerStatesReadyForTravel(ReadyLobbyPlayers));

	APRPlayerState* RiftTravelPlayerOne = NewObject<APRPlayerState>();
	TestNotNull(TEXT("Loop can create a rift travel player state copy"), RiftTravelPlayerOne);
	if (RiftTravelPlayerOne)
	{
		LobbyPlayerOne->CopyProperties(RiftTravelPlayerOne);
		TestFalse(TEXT("Ready state is reset when leaving the lobby"), RiftTravelPlayerOne->IsReady());
		TestEqual(TEXT("Role selection survives lobby-to-rift travel"), RiftTravelPlayerOne->GetSelectedRoleModule(), FName(TEXT("Assault")));
		TestEqual(TEXT("Preexisting ship resources survive lobby-to-rift travel"), RiftTravelPlayerOne->GetShipResourceCount(TEXT("CommonChip")), 1);
	}

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Loop rift test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Loop rift test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>();
	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Loop rift world owns APRRiftGameMode"), RiftGameMode);
	TestNotNull(TEXT("Loop rift world owns APRRiftGameState"), RiftGameState);
	if (!RiftGameMode || !RiftGameState)
	{
		return false;
	}

	RiftGameMode->SetReturnToLobbyServerTravelEnabled(false);

	FLoopTestPlayer RiftPlayerOne = SpawnLoopTestPlayer(*this, World, FVector::ZeroVector);
	FLoopTestPlayer RiftPlayerTwo = SpawnLoopTestPlayer(*this, World, FVector(200.0f, 0.0f, 0.0f));
	if (!RiftPlayerOne.Controller || !RiftPlayerOne.PlayerState || !RiftPlayerTwo.Controller || !RiftPlayerTwo.PlayerState)
	{
		return false;
	}

	RiftPlayerOne.PlayerState->SetReady(true);
	RiftPlayerTwo.PlayerState->SetReady(true);
	RiftPlayerOne.PlayerState->SetSelectedRoleModule(TEXT("Assault"));
	RiftPlayerTwo.PlayerState->SetSelectedRoleModule(TEXT("Support"));
	RiftPlayerOne.PlayerState->AddShipResource(TEXT("CommonChip"), 1);

	UPRInventoryComponent* PlayerOneInventory = RiftPlayerOne.PlayerState->GetInventoryComponent();
	UPRInventoryComponent* PlayerTwoInventory = RiftPlayerTwo.PlayerState->GetInventoryComponent();
	TestNotNull(TEXT("Loop player one owns inventory"), PlayerOneInventory);
	TestNotNull(TEXT("Loop player two owns inventory"), PlayerTwoInventory);
	if (!PlayerOneInventory || !PlayerTwoInventory)
	{
		return false;
	}

	TestTrue(TEXT("Loop can seed player one rift resource"), PlayerOneInventory->AddItem(MakeLoopResourceItem(TEXT("EnergyCrystal"), 3)));
	TestTrue(TEXT("Loop can seed player two rift resource"), PlayerTwoInventory->AddItem(MakeLoopResourceItem(TEXT("CommonChip"), 2)));

	RiftGameState->SetMissionTime(64.0f);
	RiftGameState->SetRiftStability(80.0f);
	RiftGameMode->UpdateAlivePlayerCount();
	TestEqual(TEXT("Loop rift tracks both alive players"), RiftGameState->GetAlivePlayerCount(), 2);
	TestTrue(TEXT("Loop rift mission starts during BeginPlay"), RiftGameMode->HasRiftMissionStarted());

	RiftGameMode->CompleteCurrentObjective();
	TestTrue(TEXT("Completing the loop objective opens extraction"), RiftGameState->IsExtractionAvailable());
	TestTrue(TEXT("First loop player can extract"), RiftGameMode->RegisterPlayerExtracted(RiftPlayerOne.Controller));
	TestFalse(TEXT("Loop waits for every alive player before completing extraction"), RiftGameState->IsExtractionComplete());
	TestTrue(TEXT("Second loop player can extract"), RiftGameMode->RegisterPlayerExtracted(RiftPlayerTwo.Controller));

	TestTrue(TEXT("Loop completes extraction once both players leave"), RiftGameState->IsExtractionComplete());
	TestTrue(TEXT("Loop creates settlement data before returning to lobby"), RiftGameState->IsSettlementReady());
	TestTrue(TEXT("Loop records return-to-lobby travel request"), RiftGameMode->IsReturnToLobbyTravelPending());

	const FPRRiftSettlementData SettlementData = RiftGameState->GetSettlementData();
	TestEqual(TEXT("Loop settlement records success"), SettlementData.Result, EPRRiftMissionResult::Success);
	TestEqual(TEXT("Loop settlement records both extracted players"), SettlementData.ExtractedPlayerCount, 2);
	TestEqual(TEXT("Loop settlement counts extracted resource items"), SettlementData.ExtractedResourceCount, 5);
	TestEqual(TEXT("Loop settlement counts extracted resource types"), SettlementData.ExtractedUniqueResourceTypes, 2);
	TestEqual(TEXT("Loop settlement has no lost resources on success"), SettlementData.LostResourceCount, 0);

	TestEqual(TEXT("Player one gains carried rift resources in ship storage"), RiftPlayerOne.PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")), 3);
	TestEqual(TEXT("Player two gains carried rift resources in ship storage"), RiftPlayerTwo.PlayerState->GetShipResourceCount(TEXT("CommonChip")), 2);
	TestEqual(TEXT("Player one keeps preexisting ship storage"), RiftPlayerOne.PlayerState->GetShipResourceCount(TEXT("CommonChip")), 1);
	TestEqual(TEXT("Player one carried rift resource is removed after extraction"), PlayerOneInventory->GetItemCount(TEXT("EnergyCrystal")), 0);
	TestEqual(TEXT("Player two carried rift resource is removed after extraction"), PlayerTwoInventory->GetItemCount(TEXT("CommonChip")), 0);

	APRPlayerState* ReturnedLobbyPlayerOne = NewObject<APRPlayerState>();
	APRPlayerState* ReturnedLobbyPlayerTwo = NewObject<APRPlayerState>();
	TestNotNull(TEXT("Loop can create returned lobby player one state"), ReturnedLobbyPlayerOne);
	TestNotNull(TEXT("Loop can create returned lobby player two state"), ReturnedLobbyPlayerTwo);
	if (ReturnedLobbyPlayerOne)
	{
		RiftPlayerOne.PlayerState->CopyProperties(ReturnedLobbyPlayerOne);
		TestFalse(TEXT("Returned lobby player one is no longer ready"), ReturnedLobbyPlayerOne->IsReady());
		TestEqual(TEXT("Returned lobby player one keeps extracted ship resource"), ReturnedLobbyPlayerOne->GetShipResourceCount(TEXT("EnergyCrystal")), 3);
		TestEqual(TEXT("Returned lobby player one keeps previous ship resource"), ReturnedLobbyPlayerOne->GetShipResourceCount(TEXT("CommonChip")), 1);
		TestEqual(TEXT("Returned lobby player one keeps selected role"), ReturnedLobbyPlayerOne->GetSelectedRoleModule(), FName(TEXT("Assault")));
	}

	if (ReturnedLobbyPlayerTwo)
	{
		RiftPlayerTwo.PlayerState->CopyProperties(ReturnedLobbyPlayerTwo);
		TestFalse(TEXT("Returned lobby player two is no longer ready"), ReturnedLobbyPlayerTwo->IsReady());
		TestEqual(TEXT("Returned lobby player two keeps extracted ship resource"), ReturnedLobbyPlayerTwo->GetShipResourceCount(TEXT("CommonChip")), 2);
		TestEqual(TEXT("Returned lobby player two keeps selected role"), ReturnedLobbyPlayerTwo->GetSelectedRoleModule(), FName(TEXT("Support")));
	}

	return true;
}

#endif
