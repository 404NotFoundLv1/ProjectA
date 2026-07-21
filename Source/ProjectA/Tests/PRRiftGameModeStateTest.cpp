#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/PRCharacter.h"
#include "Core/PRGameModeBase.h"
#include "Core/PRGameState.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Enemies/PREnemyCharacter.h"
#include "Engine/World.h"
#include "GeneralProjectSettings.h"
#include "GameMapsSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/PRPlayerState.h"
#include "Revive/PRRescueDroneActor.h"
#include "Revive/PRReviveTypes.h"
#include "Tests/PRProjectSettingsTestUtils.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRRiftGameModeStateTest, "ProjectRift.Rift.GameModeState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* LoadProjectRiftRuntimeClass(const TCHAR* ClassName, UClass* ExpectedBaseClass)
{
	const FString ClassPath = FString::Printf(TEXT("/Script/ProjectA.%s"), ClassName);
	return StaticLoadClass(ExpectedBaseClass, nullptr, *ClassPath);
}

bool TestReplicatedProperty(FAutomationTestBase& Test, UClass* Class, const FName PropertyName)
{
	FProperty* Property = Class ? Class->FindPropertyByName(PropertyName) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Class), *PropertyName.ToString()), Property);
	if (!Property)
	{
		return false;
	}

	Test.TestTrue(
		FString::Printf(TEXT("%s is replicated"), *PropertyName.ToString()),
		Property->HasAnyPropertyFlags(CPF_Net));
	return true;
}

float GetFloatPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FFloatProperty* Property = Target ? FindFProperty<FFloatProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetPropertyValue_InContainer(Target) : 0.0f;
}

int32 GetIntPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FIntProperty* Property = Target ? FindFProperty<FIntProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetPropertyValue_InContainer(Target) : 0;
}

bool GetBoolPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FBoolProperty* Property = Target ? FindFProperty<FBoolProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property && Property->GetPropertyValue_InContainer(Target);
}

}

bool FPRRiftGameModeStateTest::RunTest(const FString& Parameters)
{
	ProjectRiftTests::FScopedProjectSettingsOverride SettingsOverride;
	SettingsOverride->InitialRiftStability = 87.5f;
	SettingsOverride->RiftStabilityDrainPerSecond = 2.0f;
	SettingsOverride->FailedResourceRetentionRate = 0.25f;
	SettingsOverride->ReturnToLobbyDelayAfterSettlement = 0.25f;

	UClass* RiftGameModeClass = LoadProjectRiftRuntimeClass(TEXT("PRRiftGameMode"), AGameModeBase::StaticClass());
	UClass* RiftGameStateClass = LoadProjectRiftRuntimeClass(TEXT("PRRiftGameState"), AGameStateBase::StaticClass());

	TestNotNull(TEXT("APRRiftGameMode runtime class exists"), RiftGameModeClass);
	TestNotNull(TEXT("APRRiftGameState runtime class exists"), RiftGameStateClass);
	if (!RiftGameModeClass || !RiftGameStateClass)
	{
		return false;
	}

	TestTrue(TEXT("APRRiftGameMode derives from APRGameModeBase"), RiftGameModeClass->IsChildOf(APRGameModeBase::StaticClass()));
	TestTrue(TEXT("APRRiftGameState derives from APRGameState"), RiftGameStateClass->IsChildOf(APRGameState::StaticClass()));

	const AGameModeBase* RiftGameModeDefaults = Cast<AGameModeBase>(RiftGameModeClass->GetDefaultObject());
	TestNotNull(TEXT("APRRiftGameMode has a default object"), RiftGameModeDefaults);
	if (RiftGameModeDefaults)
	{
		TestEqual(TEXT("APRRiftGameMode uses APRRiftGameState"), RiftGameModeDefaults->GameStateClass.Get(), RiftGameStateClass);
	}

	TestNotNull(TEXT("APRRiftGameMode exposes StartRiftMission"), RiftGameModeClass->FindFunctionByName(TEXT("StartRiftMission")));
	TestNotNull(TEXT("APRRiftGameMode exposes CompleteCurrentObjective"), RiftGameModeClass->FindFunctionByName(TEXT("CompleteCurrentObjective")));
	TestNotNull(TEXT("APRRiftGameMode exposes OpenExtraction"), RiftGameModeClass->FindFunctionByName(TEXT("OpenExtraction")));
	TestNotNull(TEXT("APRRiftGameMode exposes UpdateAlivePlayerCount"), RiftGameModeClass->FindFunctionByName(TEXT("UpdateAlivePlayerCount")));
	TestNotNull(TEXT("APRRiftGameMode exposes downed notification"), RiftGameModeClass->FindFunctionByName(TEXT("HandlePlayerDowned")));
	TestNotNull(TEXT("APRRiftGameMode exposes frozen difficulty player count"), RiftGameModeClass->FindFunctionByName(TEXT("GetMissionDifficultyPlayerCount")));
	TestNotNull(TEXT("APRRiftGameMode exposes rescue drone query"), RiftGameModeClass->FindFunctionByName(TEXT("GetRescueDrone")));
	TestNotNull(TEXT("APRRiftGameMode exposes RequestRiftFailure"), RiftGameModeClass->FindFunctionByName(TEXT("RequestRiftFailure")));
	TestNotNull(TEXT("APRRiftGameMode exposes RegisterEnemyKilled"), RiftGameModeClass->FindFunctionByName(TEXT("RegisterEnemyKilled")));
	TestNotNull(TEXT("APRRiftGameMode exposes GetCurrentRunId"), RiftGameModeClass->FindFunctionByName(TEXT("GetCurrentRunId")));
	TestNotNull(TEXT("APRRiftGameMode exposes GetCurrentSettlementId"), RiftGameModeClass->FindFunctionByName(TEXT("GetCurrentSettlementId")));
	TestNotNull(TEXT("APRRiftGameMode exposes GetMissionId"), RiftGameModeClass->FindFunctionByName(TEXT("GetMissionId")));
	TestNull(TEXT("Initial stability is no longer an Actor property"), RiftGameModeClass->FindPropertyByName(TEXT("InitialRiftStability")));
	TestNull(TEXT("Stability drain is no longer an Actor property"), RiftGameModeClass->FindPropertyByName(TEXT("RiftStabilityDrainPerSecond")));
	TestNull(TEXT("Failed retention is no longer an Actor property"), RiftGameModeClass->FindPropertyByName(TEXT("FailedResourceRetentionRate")));
	TestNull(TEXT("Return delay is no longer an Actor property"), RiftGameModeClass->FindPropertyByName(TEXT("ReturnToLobbyDelayAfterSettlement")));
	TestNotNull(TEXT("APRRiftGameMode keeps its return delay getter"), RiftGameModeClass->FindFunctionByName(TEXT("GetReturnToLobbyDelayAfterSettlement")));

	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("CurrentObjectiveState"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("RiftStability"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("bExtractionAvailable"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("AlivePlayerCount"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("DifficultyPlayerCount"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("MissionTime"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("KilledEnemyCount"));

	TestNotNull(TEXT("APRRiftGameState exposes SetCurrentObjectiveState"), RiftGameStateClass->FindFunctionByName(TEXT("SetCurrentObjectiveState")));
	TestNotNull(TEXT("APRRiftGameState exposes SetRiftStability"), RiftGameStateClass->FindFunctionByName(TEXT("SetRiftStability")));
	TestNotNull(TEXT("APRRiftGameState exposes SetExtractionAvailable"), RiftGameStateClass->FindFunctionByName(TEXT("SetExtractionAvailable")));
	TestNotNull(TEXT("APRRiftGameState exposes SetAlivePlayerCount"), RiftGameStateClass->FindFunctionByName(TEXT("SetAlivePlayerCount")));
	TestNotNull(TEXT("APRRiftGameState exposes SetDifficultyPlayerCount"), RiftGameStateClass->FindFunctionByName(TEXT("SetDifficultyPlayerCount")));
	TestNotNull(TEXT("APRRiftGameState exposes SetMissionTime"), RiftGameStateClass->FindFunctionByName(TEXT("SetMissionTime")));
	TestNotNull(TEXT("APRRiftGameState exposes SetKilledEnemyCount"), RiftGameStateClass->FindFunctionByName(TEXT("SetKilledEnemyCount")));
	TestNotNull(TEXT("APRRiftGameState exposes IncrementKilledEnemyCount"), RiftGameStateClass->FindFunctionByName(TEXT("IncrementKilledEnemyCount")));
	TestNotNull(TEXT("APRRiftGameState exposes GetKilledEnemyCount"), RiftGameStateClass->FindFunctionByName(TEXT("GetKilledEnemyCount")));

	APRRiftGameState* RiftGameState = NewObject<APRRiftGameState>();
	TestNotNull(TEXT("Can instantiate APRRiftGameState"), RiftGameState);
	if (RiftGameState)
	{
		TestEqual(TEXT("Default rift stability starts full"), GetFloatPropertyValue(RiftGameState, TEXT("RiftStability")), 100.0f);
		TestFalse(TEXT("Extraction starts unavailable"), GetBoolPropertyValue(RiftGameState, TEXT("bExtractionAvailable")));
		TestEqual(TEXT("Alive player count starts at zero"), GetIntPropertyValue(RiftGameState, TEXT("AlivePlayerCount")), 0);
		TestEqual(TEXT("Difficulty player count starts at one"), GetIntPropertyValue(RiftGameState, TEXT("DifficultyPlayerCount")), 1);
		TestEqual(TEXT("Mission time starts at zero"), GetFloatPropertyValue(RiftGameState, TEXT("MissionTime")), 0.0f);
		TestEqual(TEXT("Killed enemy count starts at zero"), GetIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount")), 0);

		RiftGameState->SetRiftStability(72.5f);
		TestEqual(TEXT("Rift stability updates"), GetFloatPropertyValue(RiftGameState, TEXT("RiftStability")), 72.5f);

		RiftGameState->SetExtractionAvailable(true);
		TestTrue(TEXT("Extraction availability updates"), GetBoolPropertyValue(RiftGameState, TEXT("bExtractionAvailable")));

		RiftGameState->SetAlivePlayerCount(2);
		TestEqual(TEXT("Alive player count updates"), GetIntPropertyValue(RiftGameState, TEXT("AlivePlayerCount")), 2);
		RiftGameState->SetDifficultyPlayerCount(8);
		TestEqual(TEXT("Difficulty player count clamps to four"), RiftGameState->GetDifficultyPlayerCount(), 4);

		RiftGameState->SetMissionTime(18.0f);
		TestEqual(TEXT("Mission time updates"), GetFloatPropertyValue(RiftGameState, TEXT("MissionTime")), 18.0f);

		RiftGameState->SetKilledEnemyCount(3);
		TestEqual(TEXT("Killed enemy count updates"), GetIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount")), 3);
		RiftGameState->IncrementKilledEnemyCount();
		TestEqual(TEXT("Killed enemy count increments"), GetIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount")), 4);
	}

	UWorld* RiftTestWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/ProjectRift/Maps/L_Rift_Test.L_Rift_Test"));
	TestNotNull(TEXT("L_Rift_Test map asset loads"), RiftTestWorld);
	AWorldSettings* RiftWorldSettings = RiftTestWorld ? RiftTestWorld->GetWorldSettings() : nullptr;
	TestNotNull(TEXT("L_Rift_Test has world settings"), RiftWorldSettings);
	bool bRiftTestHasNavMeshBounds = false;
	if (RiftTestWorld && RiftTestWorld->PersistentLevel)
	{
		for (AActor* Actor : RiftTestWorld->PersistentLevel->Actors)
		{
			if (Actor && Actor->GetClass()->GetName() == TEXT("NavMeshBoundsVolume"))
			{
				bRiftTestHasNavMeshBounds = true;
				break;
			}
		}
	}
	TestTrue(TEXT("L_Rift_Test has a NavMeshBoundsVolume for enemy chase movement"), bRiftTestHasNavMeshBounds);

	const FString RiftMapConfiguredGameModePath = UGameMapsSettings::GetGameModeForMapName(TEXT("L_Rift_Test"));
	const UClass* RiftMapConfiguredGameModeClass = LoadClass<AGameModeBase>(nullptr, *RiftMapConfiguredGameModePath);
	TestTrue(
		TEXT("L_Rift_Test resolves APRRiftGameMode for runtime travel"),
		RiftMapConfiguredGameModeClass && RiftMapConfiguredGameModeClass->IsChildOf(RiftGameModeClass));

	UWorld* ObjectiveGraphTestWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/ProjectRift/Maps/L_Rift_ObjectiveGraph_Test.L_Rift_ObjectiveGraph_Test"));
	TestNotNull(TEXT("Objective graph test map asset loads"), ObjectiveGraphTestWorld);
	AWorldSettings* ObjectiveGraphWorldSettings = ObjectiveGraphTestWorld ? ObjectiveGraphTestWorld->GetWorldSettings() : nullptr;
	TestNotNull(TEXT("Objective graph test map has world settings"), ObjectiveGraphWorldSettings);
	const UClass* ObjectiveGraphGameModeClass = ObjectiveGraphWorldSettings ? ObjectiveGraphWorldSettings->DefaultGameMode : nullptr;
	TestTrue(
		TEXT("Objective graph test map explicitly uses a Rift GameMode"),
		ObjectiveGraphGameModeClass && ObjectiveGraphGameModeClass->IsChildOf(RiftGameModeClass));
	const APRRiftGameMode* ObjectiveGraphGameModeDefaults = ObjectiveGraphGameModeClass
		? Cast<APRRiftGameMode>(ObjectiveGraphGameModeClass->GetDefaultObject())
		: nullptr;
	TestEqual(
		TEXT("Objective graph test map binds its authored mission"),
		ObjectiveGraphGameModeDefaults ? ObjectiveGraphGameModeDefaults->GetMissionId() : NAME_None,
		FName(TEXT("Mission.Rift.Test.ObjectiveGraph")));

	const UGameMapsSettings* Maps = GetDefault<UGameMapsSettings>();
	const UGeneralProjectSettings* Project = GetDefault<UGeneralProjectSettings>();
	TestNotNull(TEXT("Game maps settings are available"), Maps);
	TestNotNull(TEXT("General project settings are available"), Project);
	if (Maps && Project)
	{
		TestEqual(TEXT("Game default map is ship lobby"), Maps->GetGameDefaultMap(), FString(TEXT("/Game/ProjectRift/Maps/L_ShipLobby")));
		TestEqual(TEXT("Project name is ProjectRift"), Project->ProjectName, FString(TEXT("ProjectRift")));
		TestEqual(TEXT("Project version is v0.8.2"), Project->ProjectVersion, FString(TEXT("0.8.2")));
	}

	TArray<FString> MapsToCook;
	GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MapsToCook"), MapsToCook, GGameIni);
	const auto ContainsCookMap = [&MapsToCook](const TCHAR* MapPath)
	{
		return MapsToCook.ContainsByPredicate([MapPath](const FString& Entry)
		{
			return Entry.Contains(MapPath);
		});
	};
	TestTrue(TEXT("Cook list contains main menu"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_MainMenu")));
	TestTrue(TEXT("Cook list contains ship lobby"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_ShipLobby")));
	TestTrue(TEXT("Cook list contains rift test"), ContainsCookMap(TEXT("/Game/ProjectRift/Maps/L_Rift_Test")));

	FTestWorldWrapper StabilityWorldWrapper;
	TestTrue(TEXT("Rift stability failure test world is created"), StabilityWorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* StabilityWorld = StabilityWorldWrapper.GetTestWorld();
	if (!StabilityWorld)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = StabilityWorld->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Rift stability failure test world begins play"), StabilityWorldWrapper.BeginPlayInTestWorld());
	if (StabilityWorldWrapper.HasFailed())
	{
		StabilityWorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameMode* StabilityGameMode = StabilityWorld->GetAuthGameMode<APRRiftGameMode>();
	APRRiftGameState* StabilityGameState = StabilityWorld->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Stability failure world owns APRRiftGameMode"), StabilityGameMode);
	TestNotNull(TEXT("Stability failure world owns APRRiftGameState"), StabilityGameState);
	if (!StabilityGameMode || !StabilityGameState)
	{
		return false;
	}
	TestEqual(TEXT("Mission starts from project settings stability"), StabilityGameState->GetRiftStability(), 87.5f);
	TestEqual(TEXT("Return delay comes from project settings"), StabilityGameMode->GetReturnToLobbyDelayAfterSettlement(), 0.25f);
	SettingsOverride->ReturnToLobbyDelayAfterSettlement = -2.0f;
	TestEqual(TEXT("Negative return delay clamps to zero"), StabilityGameMode->GetReturnToLobbyDelayAfterSettlement(), 0.0f);
	SettingsOverride->ReturnToLobbyDelayAfterSettlement = 0.25f;
	SettingsOverride->FailedResourceRetentionRate = 2.0f;
	TestEqual(TEXT("Failed retention clamps above one"), StabilityGameMode->CalculateRetainedResourceCount(4, EPRRiftMissionResult::Failed), 4);
	SettingsOverride->FailedResourceRetentionRate = -1.0f;
	TestEqual(TEXT("Failed retention clamps below zero"), StabilityGameMode->CalculateRetainedResourceCount(4, EPRRiftMissionResult::Failed), 0);
	SettingsOverride->FailedResourceRetentionRate = 0.25f;

	const FGuid FirstRunId = StabilityGameMode->GetCurrentRunId();
	TestTrue(TEXT("BeginPlay creates a valid run id"), FirstRunId.IsValid());
	TestTrue(TEXT("Mission owns a valid settlement id"), StabilityGameMode->GetCurrentSettlementId().IsValid());
	TestFalse(TEXT("Mission id is configured"), StabilityGameMode->GetMissionId().IsNone());
	SettingsOverride->InitialRiftStability = 150.0f;
	TestTrue(TEXT("Restarting with high stability succeeds"), StabilityGameMode->StartRiftMission());
	TestEqual(TEXT("Initial stability clamps above 100"), StabilityGameState->GetRiftStability(), 100.0f);
	TestNotEqual(TEXT("Restarting rotates the run id"), StabilityGameMode->GetCurrentRunId(), FirstRunId);
	SettingsOverride->InitialRiftStability = -10.0f;
	TestTrue(TEXT("Restarting with negative stability succeeds"), StabilityGameMode->StartRiftMission());
	TestEqual(TEXT("Initial stability clamps below zero"), StabilityGameState->GetRiftStability(), 0.0f);
	SettingsOverride->InitialRiftStability = 87.5f;
	TestTrue(TEXT("Restoring approved test stability succeeds"), StabilityGameMode->StartRiftMission());
	TestEqual(TEXT("Restored initial stability is applied"), StabilityGameState->GetRiftStability(), 87.5f);

	APREnemyCharacter* CountedEnemy = StabilityWorld->SpawnActor<APREnemyCharacter>();
	TestNotNull(TEXT("Can spawn enemy for duplicate kill test"), CountedEnemy);
	if (CountedEnemy)
	{
		TestTrue(TEXT("First enemy death notification is counted"), StabilityGameMode->RegisterEnemyKilled(CountedEnemy, nullptr));
		TestFalse(TEXT("Duplicate enemy death notification is ignored"), StabilityGameMode->RegisterEnemyKilled(CountedEnemy, nullptr));
		TestEqual(TEXT("Duplicate enemy notification leaves kill count at one"), StabilityGameState->GetKilledEnemyCount(), 1);
	}

	StabilityGameMode->SetReturnToLobbyServerTravelEnabled(false);
	StabilityGameState->SetCurrentObjectiveState(EPRRiftObjectiveState::Active);
	StabilityGameState->SetRiftStability(1.0f);
	SettingsOverride->RiftStabilityDrainPerSecond = -1.0f;
	StabilityGameMode->Tick(0.75f);
	TestEqual(TEXT("Negative stability drain clamps to zero"), StabilityGameState->GetRiftStability(), 1.0f);
	TestFalse(TEXT("Clamped negative drain does not fail the mission"), StabilityGameState->IsSettlementReady());
	SettingsOverride->RiftStabilityDrainPerSecond = 2.0f;
	StabilityGameMode->Tick(0.75f);
	TestTrue(TEXT("Stability depletion produces a settlement"), StabilityGameState->IsSettlementReady());
	TestEqual(TEXT("Stability depletion fails the mission"), StabilityGameState->GetSettlementData().Result, EPRRiftMissionResult::Failed);
	TestEqual(TEXT("Stability depletion marks the objective failed"), StabilityGameState->GetCurrentObjectiveState(), EPRRiftObjectiveState::Failed);
	TestEqual(TEXT("Stability depletion clamps rift stability to zero"), StabilityGameState->GetRiftStability(), 0.0f);
	TestTrue(TEXT("Stability depletion requests lobby return travel"), StabilityGameMode->IsReturnToLobbyTravelPending());

	FTestWorldWrapper DownedWorldWrapper;
	TestTrue(TEXT("All-downed failure test world is created"), DownedWorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* DownedWorld = DownedWorldWrapper.GetTestWorld();
	if (!DownedWorld)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = DownedWorld->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("All-downed failure test world begins play"), DownedWorldWrapper.BeginPlayInTestWorld());
	if (DownedWorldWrapper.HasFailed())
	{
		DownedWorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameMode* DownedGameMode = DownedWorld->GetAuthGameMode<APRRiftGameMode>();
	APRRiftGameState* DownedGameState = DownedWorld->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("All-downed world owns APRRiftGameMode"), DownedGameMode);
	TestNotNull(TEXT("All-downed world owns APRRiftGameState"), DownedGameState);
	if (!DownedGameMode || !DownedGameState)
	{
		return false;
	}

	DownedGameMode->SetReturnToLobbyServerTravelEnabled(false);
	APlayerController* PlayerController = DownedWorld->SpawnActor<APlayerController>();
	APRPlayerState* PlayerState = DownedWorld->SpawnActor<APRPlayerState>();
	APRCharacter* PlayerCharacter = DownedWorld->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	TestNotNull(TEXT("All-downed player controller spawned"), PlayerController);
	TestNotNull(TEXT("All-downed player state spawned"), PlayerState);
	TestNotNull(TEXT("All-downed player character spawned"), PlayerCharacter);
	if (!PlayerController || !PlayerState || !PlayerCharacter)
	{
		return false;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(PlayerCharacter);
	TestTrue(TEXT("All-downed player initializes ASC"), PlayerCharacter->InitializeAbilitySystemFromPlayerState(PlayerState));
	if (!DownedGameState->PlayerArray.Contains(PlayerState))
	{
		DownedGameState->AddPlayerState(PlayerState);
	}
	for (TObjectPtr<APlayerState>& ExistingPlayerState : DownedGameState->PlayerArray)
	{
		if (ExistingPlayerState && ExistingPlayerState != PlayerState)
		{
			ExistingPlayerState->SetIsOnlyASpectator(true);
		}
	}

	DownedGameMode->UpdateAlivePlayerCount();
	TestEqual(TEXT("Living possessed players count as alive before downed state"), DownedGameState->GetAlivePlayerCount(), 1);
	TestTrue(TEXT("Restarting after player registration freezes a solo mission"), DownedGameMode->StartRiftMission());
	TestEqual(TEXT("Solo mission freezes one difficulty player"), DownedGameMode->GetMissionDifficultyPlayerCount(), 1);
	APRRescueDroneActor* RescueDrone = DownedGameMode->GetRescueDrone();
	TestNotNull(TEXT("Solo mission spawns one rescue drone"), RescueDrone);
	TestTrue(TEXT("Can put the only rift player into downed state"), PlayerCharacter->EnterDownedState());
	DownedGameMode->UpdateAlivePlayerCount();
	TestEqual(TEXT("Downed players no longer count as alive"), DownedGameState->GetAlivePlayerCount(), 0);
	TestFalse(TEXT("Ready solo drone prevents immediate mission failure"), DownedGameState->IsSettlementReady());
	TestEqual(TEXT("Solo drone deploys for the first down"), RescueDrone ? RescueDrone->GetDroneState() : EPRRescueDroneState::Unavailable, EPRRescueDroneState::Deployed);
	for (int32 TickIndex = 0; TickIndex < 36; ++TickIndex)
	{
		++GFrameCounter;
		DownedWorld->Tick(ELevelTick::LEVELTICK_All, 0.1f);
	}
	TestTrue(TEXT("Solo drone completes the first rescue"), PlayerCharacter->IsAlive());
	TestEqual(TEXT("Solo drone spends its only rescue"), RescueDrone ? RescueDrone->GetDroneState() : EPRRescueDroneState::Unavailable, EPRRescueDroneState::Spent);
	TestTrue(TEXT("Second solo down is accepted"), PlayerCharacter->EnterDownedState());
	DownedGameMode->UpdateAlivePlayerCount();
	TestTrue(TEXT("Spent solo drone allows second down to fail the mission"), DownedGameState->IsSettlementReady());
	TestEqual(TEXT("Second solo down produces a failed settlement"), DownedGameState->GetSettlementData().Result, EPRRiftMissionResult::Failed);

	return true;
}

#endif
