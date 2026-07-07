#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRRiftSpawnManagerTest, "ProjectRift.Rift.SpawnManager", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* LoadRiftSpawnRuntimeClass(const TCHAR* ClassName, UClass* ExpectedBaseClass)
{
	const FString ClassPath = FString::Printf(TEXT("/Script/ProjectA.%s"), ClassName);
	return StaticLoadClass(ExpectedBaseClass, nullptr, *ClassPath);
}

bool SetIntPropertyValue(UObject* Target, const FName PropertyName, const int32 Value)
{
	FIntProperty* Property = Target ? FindFProperty<FIntProperty>(Target->GetClass(), PropertyName) : nullptr;
	if (!Property)
	{
		return false;
	}

	Property->SetPropertyValue_InContainer(Target, Value);
	return true;
}

bool SetFloatPropertyValue(UObject* Target, const FName PropertyName, const float Value)
{
	FFloatProperty* Property = Target ? FindFProperty<FFloatProperty>(Target->GetClass(), PropertyName) : nullptr;
	if (!Property)
	{
		return false;
	}

	Property->SetPropertyValue_InContainer(Target, Value);
	return true;
}

bool CallBoolFunctionNoParams(UObject* Target, const FName FunctionName, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallIntFunctionNoParams(UObject* Target, const FName FunctionName, int32& OutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FIntProperty* ReturnValue = FindFProperty<FIntProperty>(Function, TEXT("ReturnValue")))
	{
		OutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallIntFunctionNoParams(UObject* Target, const FName FunctionName, FAutomationTestBase& Test, const TCHAR* FailureLabel, int32& OutResult)
{
	const bool bCalled = CallIntFunctionNoParams(Target, FunctionName, OutResult);
	Test.TestTrue(FailureLabel, bCalled);
	return bCalled;
}

bool CallVoidFunctionNoParams(UObject* Target, const FName FunctionName)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	Target->ProcessEvent(Function, Params.GetStructMemory());
	return true;
}

bool CallBoolFunctionWithObjectParam(UObject* Target, const FName FunctionName, const FName ParamName, UObject* ParamValue, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	if (FObjectProperty* ObjectParam = FindFProperty<FObjectProperty>(Function, ParamName))
	{
		ObjectParam->SetObjectPropertyValue_InContainer(ParamsMemory, ParamValue);
	}

	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}
}

bool FPRRiftSpawnManagerTest::RunTest(const FString& Parameters)
{
	UClass* SpawnManagerClass = LoadRiftSpawnRuntimeClass(TEXT("PRSpawnManager"), AActor::StaticClass());
	UClass* SpawnPointClass = LoadRiftSpawnRuntimeClass(TEXT("PREnemySpawnPoint"), AActor::StaticClass());
	UClass* RiftGameModeClass = APRRiftGameMode::StaticClass();

	TestNotNull(TEXT("APRSpawnManager runtime class exists"), SpawnManagerClass);
	TestNotNull(TEXT("APREnemySpawnPoint runtime class exists"), SpawnPointClass);
	if (!SpawnManagerClass || !SpawnPointClass)
	{
		return false;
	}

	TestTrue(TEXT("APRSpawnManager derives from AActor"), SpawnManagerClass->IsChildOf(AActor::StaticClass()));
	TestTrue(TEXT("APREnemySpawnPoint derives from AActor"), SpawnPointClass->IsChildOf(AActor::StaticClass()));
	TestNotNull(TEXT("APRSpawnManager exposes StartSpawningForObjective"), SpawnManagerClass->FindFunctionByName(TEXT("StartSpawningForObjective")));
	TestNotNull(TEXT("APRSpawnManager exposes StopSpawning"), SpawnManagerClass->FindFunctionByName(TEXT("StopSpawning")));
	TestNotNull(TEXT("APRSpawnManager exposes SpawnWave"), SpawnManagerClass->FindFunctionByName(TEXT("SpawnWave")));
	TestNotNull(TEXT("APRSpawnManager exposes GetAliveEnemyCount"), SpawnManagerClass->FindFunctionByName(TEXT("GetAliveEnemyCount")));
	TestNotNull(TEXT("APRSpawnManager exposes GetSpawnedWaveCount"), SpawnManagerClass->FindFunctionByName(TEXT("GetSpawnedWaveCount")));
	TestNotNull(TEXT("APRSpawnManager exposes IsSpawningActive"), SpawnManagerClass->FindFunctionByName(TEXT("IsSpawningActive")));
	TestNotNull(TEXT("APRSpawnManager exposes DiscoverSpawnPoints"), SpawnManagerClass->FindFunctionByName(TEXT("DiscoverSpawnPoints")));
	TestNotNull(TEXT("APREnemySpawnPoint exposes GetSpawnTransform"), SpawnPointClass->FindFunctionByName(TEXT("GetSpawnTransform")));
	TestNotNull(TEXT("APRRiftGameMode exposes StartSpawnManagersForObjective"), RiftGameModeClass->FindFunctionByName(TEXT("StartSpawnManagersForObjective")));
	TestNotNull(TEXT("APRRiftGameMode exposes StopSpawnManagers"), RiftGameModeClass->FindFunctionByName(TEXT("StopSpawnManagers")));

	UFunction* StartSpawningFunction = SpawnManagerClass->FindFunctionByName(TEXT("StartSpawningForObjective"));
	TestTrue(
		TEXT("StartSpawningForObjective is authority-only"),
		StartSpawningFunction && StartSpawningFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	UFunction* SpawnWaveFunction = SpawnManagerClass->FindFunctionByName(TEXT("SpawnWave"));
	TestTrue(
		TEXT("SpawnWave is authority-only"),
		SpawnWaveFunction && SpawnWaveFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

	const AActor* SpawnManagerDefaults = Cast<AActor>(SpawnManagerClass->GetDefaultObject());
	TestNotNull(TEXT("APRSpawnManager has a default object"), SpawnManagerDefaults);
	if (SpawnManagerDefaults)
	{
		TestFalse(TEXT("APRSpawnManager uses timers instead of per-frame Tick"), SpawnManagerDefaults->PrimaryActorTick.bCanEverTick);
		TestTrue(TEXT("APRSpawnManager replicates its active state to clients"), SpawnManagerDefaults->GetIsReplicated());
	}

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Rift spawn manager test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Rift spawn manager test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Rift spawn manager test world owns APRRiftGameState"), RiftGameState);
	if (RiftGameState)
	{
		RiftGameState->SetAlivePlayerCount(3);
	}

	AActor* SpawnPointA = World->SpawnActor<AActor>(SpawnPointClass, FVector(500.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
	AActor* SpawnPointB = World->SpawnActor<AActor>(SpawnPointClass, FVector(800.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
	AActor* SpawnManager = World->SpawnActor<AActor>(SpawnManagerClass, FVector::ZeroVector, FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn first enemy spawn point"), SpawnPointA);
	TestNotNull(TEXT("Can spawn second enemy spawn point"), SpawnPointB);
	TestNotNull(TEXT("Can spawn rift spawn manager"), SpawnManager);
	if (!RiftGameState || !SpawnPointA || !SpawnPointB || !SpawnManager)
	{
		return false;
	}

	TestTrue(TEXT("Can configure base enemies per wave"), SetIntPropertyValue(SpawnManager, TEXT("BaseEnemiesPerWave"), 1));
	TestTrue(TEXT("Can configure enemies per extra player"), SetIntPropertyValue(SpawnManager, TEXT("EnemiesPerAdditionalPlayer"), 1));
	TestTrue(TEXT("Can configure max alive enemies"), SetIntPropertyValue(SpawnManager, TEXT("MaxAliveEnemies"), 3));
	TestTrue(TEXT("Can configure wave interval"), SetFloatPropertyValue(SpawnManager, TEXT("WaveInterval"), 0.2f));

	bool bActiveBeforeObjective = true;
	TestTrue(TEXT("Can query inactive spawn manager state"), CallBoolFunctionNoParams(SpawnManager, TEXT("IsSpawningActive"), bActiveBeforeObjective));
	TestFalse(TEXT("Spawn manager starts inactive"), bActiveBeforeObjective);

	APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>();
	TestNotNull(TEXT("Test world owns APRRiftGameMode"), RiftGameMode);
	if (!RiftGameMode)
	{
		return false;
	}

	bool bStarted = false;
	TestTrue(
		TEXT("GameMode can start spawn managers through reflected API"),
		CallBoolFunctionWithObjectParam(RiftGameMode, TEXT("StartSpawnManagersForObjective"), TEXT("ObjectiveActor"), nullptr, bStarted));
	TestTrue(TEXT("GameMode starts at least one spawn manager"), bStarted);

	bool bActiveAfterObjective = false;
	TestTrue(TEXT("Can query active spawn manager state"), CallBoolFunctionNoParams(SpawnManager, TEXT("IsSpawningActive"), bActiveAfterObjective));
	TestTrue(TEXT("Spawn manager activates when a rift objective starts"), bActiveAfterObjective);

	int32 SpawnedWaveCount = 0;
	CallIntFunctionNoParams(SpawnManager, TEXT("GetSpawnedWaveCount"), *this, TEXT("Can query spawned wave count"), SpawnedWaveCount);
	TestEqual(TEXT("Starting the objective immediately spawns the first wave"), SpawnedWaveCount, 1);

	int32 AliveEnemyCount = 0;
	CallIntFunctionNoParams(SpawnManager, TEXT("GetAliveEnemyCount"), *this, TEXT("Can query alive enemy count"), AliveEnemyCount);
	TestEqual(TEXT("First wave scales from alive player count and respects max alive cap"), AliveEnemyCount, 3);

	int32 ReplicatedSpawnedActors = 0;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Candidate = *ActorIt;
		if (Candidate && Candidate->GetOwner() == SpawnManager)
		{
			++ReplicatedSpawnedActors;
			TestTrue(TEXT("Spawned enemy actor is replicated for multiplayer clients"), Candidate->GetIsReplicated());
			TestTrue(TEXT("Spawned enemy actor replicates movement"), Candidate->IsReplicatingMovement());
		}
	}
	TestEqual(TEXT("All alive spawned enemies are owned by the spawn manager"), ReplicatedSpawnedActors, AliveEnemyCount);

	int32 ManualWaveSpawned = 0;
	CallIntFunctionNoParams(SpawnManager, TEXT("SpawnWave"), *this, TEXT("Can manually request a wave"), ManualWaveSpawned);
	TestEqual(TEXT("Spawn manager refuses to exceed max alive enemy cap"), ManualWaveSpawned, 0);

	TestTrue(TEXT("GameMode can stop spawn managers through reflected API"), CallVoidFunctionNoParams(RiftGameMode, TEXT("StopSpawnManagers")));
	bool bActiveAfterStop = true;
	TestTrue(TEXT("Can query stopped spawn manager state"), CallBoolFunctionNoParams(SpawnManager, TEXT("IsSpawningActive"), bActiveAfterStop));
	TestFalse(TEXT("Spawn manager stops when the objective flow ends"), bActiveAfterStop);

	return true;
}

#endif
