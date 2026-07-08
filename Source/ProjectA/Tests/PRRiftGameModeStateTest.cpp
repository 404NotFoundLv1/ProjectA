#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRGameModeBase.h"
#include "Core/PRGameState.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
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

	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("CurrentObjectiveState"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("RiftStability"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("bExtractionAvailable"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("AlivePlayerCount"));
	TestReplicatedProperty(*this, RiftGameStateClass, TEXT("MissionTime"));

	TestNotNull(TEXT("APRRiftGameState exposes SetCurrentObjectiveState"), RiftGameStateClass->FindFunctionByName(TEXT("SetCurrentObjectiveState")));
	TestNotNull(TEXT("APRRiftGameState exposes SetRiftStability"), RiftGameStateClass->FindFunctionByName(TEXT("SetRiftStability")));
	TestNotNull(TEXT("APRRiftGameState exposes SetExtractionAvailable"), RiftGameStateClass->FindFunctionByName(TEXT("SetExtractionAvailable")));
	TestNotNull(TEXT("APRRiftGameState exposes SetAlivePlayerCount"), RiftGameStateClass->FindFunctionByName(TEXT("SetAlivePlayerCount")));
	TestNotNull(TEXT("APRRiftGameState exposes SetMissionTime"), RiftGameStateClass->FindFunctionByName(TEXT("SetMissionTime")));

	APRRiftGameState* RiftGameState = NewObject<APRRiftGameState>();
	TestNotNull(TEXT("Can instantiate APRRiftGameState"), RiftGameState);
	if (RiftGameState)
	{
		TestEqual(TEXT("Default rift stability starts full"), GetFloatPropertyValue(RiftGameState, TEXT("RiftStability")), 100.0f);
		TestFalse(TEXT("Extraction starts unavailable"), GetBoolPropertyValue(RiftGameState, TEXT("bExtractionAvailable")));
		TestEqual(TEXT("Alive player count starts at zero"), GetIntPropertyValue(RiftGameState, TEXT("AlivePlayerCount")), 0);
		TestEqual(TEXT("Mission time starts at zero"), GetFloatPropertyValue(RiftGameState, TEXT("MissionTime")), 0.0f);

		RiftGameState->SetRiftStability(72.5f);
		TestEqual(TEXT("Rift stability updates"), GetFloatPropertyValue(RiftGameState, TEXT("RiftStability")), 72.5f);

		RiftGameState->SetExtractionAvailable(true);
		TestTrue(TEXT("Extraction availability updates"), GetBoolPropertyValue(RiftGameState, TEXT("bExtractionAvailable")));

		RiftGameState->SetAlivePlayerCount(2);
		TestEqual(TEXT("Alive player count updates"), GetIntPropertyValue(RiftGameState, TEXT("AlivePlayerCount")), 2);

		RiftGameState->SetMissionTime(18.0f);
		TestEqual(TEXT("Mission time updates"), GetFloatPropertyValue(RiftGameState, TEXT("MissionTime")), 18.0f);
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

	return true;
}

#endif
