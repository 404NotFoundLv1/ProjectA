#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/WidgetComponent.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Player/PRPlayerController.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRRiftObjectiveSystemTest, "ProjectRift.Rift.ObjectiveSystem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* LoadRiftObjectiveTestClass(const TCHAR* ClassName, UClass* ExpectedBaseClass)
{
	const FString ClassPath = FString::Printf(TEXT("/Script/ProjectA.%s"), ClassName);
	return StaticLoadClass(ExpectedBaseClass, nullptr, *ClassPath);
}

bool TestRiftObjectiveReplicatedProperty(FAutomationTestBase& Test, UClass* Class, const FName PropertyName)
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

bool CallTextFunctionNoParams(UObject* Target, const FName FunctionName, FText& OutText)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FTextProperty* ReturnValue = FindFProperty<FTextProperty>(Function, TEXT("ReturnValue")))
	{
		OutText = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

float GetRiftObjectiveFloatProperty(const UObject* Target, const FName PropertyName)
{
	const FFloatProperty* Property = Target ? FindFProperty<FFloatProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetPropertyValue_InContainer(Target) : 0.0f;
}

uint8 GetRiftObjectiveEnumProperty(const UObject* Target, const FName PropertyName)
{
	const FEnumProperty* EnumProperty = Target ? FindFProperty<FEnumProperty>(Target->GetClass(), PropertyName) : nullptr;
	if (EnumProperty)
	{
		return EnumProperty->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Target));
	}

	const FByteProperty* ByteProperty = Target ? FindFProperty<FByteProperty>(Target->GetClass(), PropertyName) : nullptr;
	return ByteProperty ? ByteProperty->GetPropertyValue_InContainer(Target) : 0;
}
}

bool FPRRiftObjectiveSystemTest::RunTest(const FString& Parameters)
{
	UClass* RiftObjectiveClass = LoadRiftObjectiveTestClass(TEXT("PRRiftObjectiveActor"), AActor::StaticClass());
	UClass* HoldObjectiveClass = LoadRiftObjectiveTestClass(TEXT("PRHoldObjectiveActor"), AActor::StaticClass());

	TestNotNull(TEXT("APRRiftObjectiveActor runtime class exists"), RiftObjectiveClass);
	TestNotNull(TEXT("APRHoldObjectiveActor runtime class exists"), HoldObjectiveClass);
	if (!RiftObjectiveClass || !HoldObjectiveClass)
	{
		return false;
	}

	TestTrue(TEXT("APRHoldObjectiveActor derives from APRRiftObjectiveActor"), HoldObjectiveClass->IsChildOf(RiftObjectiveClass));
	TestNotNull(TEXT("Objective exposes ActivateObjective"), RiftObjectiveClass->FindFunctionByName(TEXT("ActivateObjective")));
	TestNotNull(TEXT("Objective exposes CanActivateObjective"), RiftObjectiveClass->FindFunctionByName(TEXT("CanActivateObjective")));
	TestNotNull(TEXT("Objective exposes CompleteObjective"), RiftObjectiveClass->FindFunctionByName(TEXT("CompleteObjective")));
	TestNotNull(TEXT("Objective exposes GetInteractionPromptText"), RiftObjectiveClass->FindFunctionByName(TEXT("GetInteractionPromptText")));
	TestNotNull(TEXT("Hold objective exposes GetHoldDuration"), HoldObjectiveClass->FindFunctionByName(TEXT("GetHoldDuration")));
	TestNotNull(TEXT("Hold objective exposes GetCurrentHoldTime"), HoldObjectiveClass->FindFunctionByName(TEXT("GetCurrentHoldTime")));

	TestRiftObjectiveReplicatedProperty(*this, RiftObjectiveClass, TEXT("ObjectiveState"));
	TestRiftObjectiveReplicatedProperty(*this, RiftObjectiveClass, TEXT("ObjectiveProgress"));
	TestRiftObjectiveReplicatedProperty(*this, HoldObjectiveClass, TEXT("CurrentHoldTime"));

	AActor* RiftObjectiveDefaults = Cast<AActor>(RiftObjectiveClass->GetDefaultObject());
	const UWidgetComponent* ObjectivePromptDefaults = RiftObjectiveDefaults
		? FindObject<UWidgetComponent>(RiftObjectiveDefaults, TEXT("InteractionPromptWidget"))
		: nullptr;
	TestNotNull(TEXT("Rift objective owns a screen-space interaction prompt widget"), ObjectivePromptDefaults);
	TestTrue(TEXT("Rift objective prompt widget is hidden until a pawn is nearby"), ObjectivePromptDefaults && ObjectivePromptDefaults->bHiddenInGame);
	TestEqual(
		TEXT("Rift objective prompt uses screen-space widget rendering for localized text"),
		ObjectivePromptDefaults ? ObjectivePromptDefaults->GetWidgetSpace() : EWidgetSpace::World,
		EWidgetSpace::Screen);
	TestTrue(
		TEXT("Rift objective prompt widget has a native widget class"),
		ObjectivePromptDefaults && ObjectivePromptDefaults->GetWidgetClass() != nullptr);
	FText DefaultPromptText;
	TestTrue(
		TEXT("Rift objective prompt tells the player to press F to start the task"),
		CallTextFunctionNoParams(RiftObjectiveDefaults, TEXT("GetInteractionPromptText"), DefaultPromptText)
		&& DefaultPromptText.ToString().Contains(TEXT("\u5F00\u59CB\u4EFB\u52A1")));
	TestTrue(
		TEXT("Rift objective prompt is lifted above the objective marker"),
		ObjectivePromptDefaults && ObjectivePromptDefaults->GetRelativeLocation().Z >= 260.0f);

	UClass* HoldObjectiveBlueprintClass = StaticLoadClass(
		AActor::StaticClass(),
		nullptr,
		TEXT("/Game/ProjectRift/Blueprints/Rift/BP_HoldObjective.BP_HoldObjective_C"));
	TestNotNull(TEXT("BP_HoldObjective blueprint class loads"), HoldObjectiveBlueprintClass);
	if (HoldObjectiveBlueprintClass)
	{
		AActor* BlueprintObjectiveDefaults = Cast<AActor>(HoldObjectiveBlueprintClass->GetDefaultObject());
		const UWidgetComponent* BlueprintObjectivePromptDefaults = BlueprintObjectiveDefaults
			? FindObject<UWidgetComponent>(BlueprintObjectiveDefaults, TEXT("InteractionPromptWidget"))
			: nullptr;
		TestNotNull(TEXT("BP_HoldObjective owns inherited interaction prompt widget"), BlueprintObjectivePromptDefaults);
		TestTrue(
			TEXT("BP_HoldObjective prompt is lifted above the objective marker"),
			BlueprintObjectivePromptDefaults && BlueprintObjectivePromptDefaults->GetRelativeLocation().Z >= 260.0f);
	}

	UClass* RiftGameStateClass = APRRiftGameState::StaticClass();
	TestRiftObjectiveReplicatedProperty(*this, RiftGameStateClass, TEXT("ObjectiveProgress"));
	TestNotNull(TEXT("Rift GameState exposes SetObjectiveProgress"), RiftGameStateClass->FindFunctionByName(TEXT("SetObjectiveProgress")));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("PlayerController exposes TryActivateRiftObjective"), PlayerControllerClass->FindFunctionByName(TEXT("TryActivateRiftObjective")));
	TestNotNull(TEXT("PlayerController exposes FindBestRiftObjectiveCandidate"), PlayerControllerClass->FindFunctionByName(TEXT("FindBestRiftObjectiveCandidate")));
	UFunction* ServerTryActivateFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerTryActivateRiftObjective"));
	TestNotNull(TEXT("PlayerController exposes ServerTryActivateRiftObjective RPC"), ServerTryActivateFunction);
	TestTrue(
		TEXT("ServerTryActivateRiftObjective is server-authoritative"),
		ServerTryActivateFunction && ServerTryActivateFunction->HasAnyFunctionFlags(FUNC_NetServer));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Rift objective test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Rift objective test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Rift objective test world owns APRRiftGameState"), RiftGameState);

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>(FVector::ZeroVector, FRotator::ZeroRotator);
	AActor* HoldObjective = World->SpawnActor<AActor>(HoldObjectiveClass, FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn test rift player controller"), PlayerController);
	TestNotNull(TEXT("Can spawn hold objective actor"), HoldObjective);
	if (!RiftGameState || !PlayerController || !HoldObjective)
	{
		return false;
	}

	ADefaultPawn* NearbyPawn = World->SpawnActor<ADefaultPawn>(FVector(140.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn nearby pawn for objective prompt"), NearbyPawn);
	if (NearbyPawn)
	{
		PlayerController->Possess(NearbyPawn);
	}

	UWidgetComponent* RuntimeObjectivePromptWidget = FindObject<UWidgetComponent>(HoldObjective, TEXT("InteractionPromptWidget"));
	if (RuntimeObjectivePromptWidget)
	{
		RuntimeObjectivePromptWidget->SetHiddenInGame(true);
		RuntimeObjectivePromptWidget->SetVisibility(false, true);
	}
	HoldObjective->Tick(0.0f);
	TestFalse(
		TEXT("Rift objective prompt widget becomes visible when a pawn is already nearby"),
		RuntimeObjectivePromptWidget ? RuntimeObjectivePromptWidget->bHiddenInGame : true);
	TestTrue(
		TEXT("Rift objective prompt widget component becomes visible when a pawn is already nearby"),
		RuntimeObjectivePromptWidget && RuntimeObjectivePromptWidget->IsVisible());

	if (HoldObjectiveBlueprintClass)
	{
		AActor* BlueprintHoldObjective = World->SpawnActor<AActor>(HoldObjectiveBlueprintClass, FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		ADefaultPawn* BlueprintNearbyPawn = World->SpawnActor<ADefaultPawn>(FVector(540.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		TestNotNull(TEXT("Can spawn BP_HoldObjective in test world"), BlueprintHoldObjective);
		TestNotNull(TEXT("Can spawn nearby pawn for BP_HoldObjective prompt"), BlueprintNearbyPawn);
		if (BlueprintNearbyPawn)
		{
			PlayerController->Possess(BlueprintNearbyPawn);
		}

		UWidgetComponent* BlueprintRuntimePromptWidget = FindObject<UWidgetComponent>(BlueprintHoldObjective, TEXT("InteractionPromptWidget"));
		if (BlueprintRuntimePromptWidget)
		{
			BlueprintRuntimePromptWidget->SetHiddenInGame(true);
			BlueprintRuntimePromptWidget->SetVisibility(false, true);
		}
		if (BlueprintHoldObjective)
		{
			BlueprintHoldObjective->Tick(0.0f);
		}
		TestFalse(
			TEXT("BP_HoldObjective prompt widget becomes visible when a pawn is already nearby"),
			BlueprintRuntimePromptWidget ? BlueprintRuntimePromptWidget->bHiddenInGame : true);
		TestTrue(
			TEXT("BP_HoldObjective prompt widget component becomes visible when a pawn is already nearby"),
			BlueprintRuntimePromptWidget && BlueprintRuntimePromptWidget->IsVisible());
	}

	bool bActivated = false;
	TestTrue(
		TEXT("Can activate hold objective through reflected API"),
		CallBoolFunctionWithObjectParam(HoldObjective, TEXT("ActivateObjective"), TEXT("ActivatingController"), PlayerController, bActivated));
	TestTrue(TEXT("Hold objective activation succeeds"), bActivated);

	TestEqual(TEXT("Objective starts active"), GetRiftObjectiveEnumProperty(HoldObjective, TEXT("ObjectiveState")), static_cast<uint8>(EPRRiftObjectiveState::Active));
	TestEqual(TEXT("GameState objective starts active"), static_cast<uint8>(RiftGameState->GetCurrentObjectiveState()), static_cast<uint8>(EPRRiftObjectiveState::Active));

	FText ActivePromptText;
	TestTrue(
		TEXT("Activated objective prompt changes to an in-progress message"),
		CallTextFunctionNoParams(HoldObjective, TEXT("GetInteractionPromptText"), ActivePromptText)
		&& ActivePromptText.ToString().Contains(TEXT("\u4EFB\u52A1\u8FDB\u884C\u4E2D")));

	HoldObjective->Tick(15.0f);
	TestEqual(TEXT("Hold objective tracks elapsed hold time"), GetRiftObjectiveFloatProperty(HoldObjective, TEXT("CurrentHoldTime")), 15.0f);
	TestEqual(TEXT("Hold objective reports half progress"), GetRiftObjectiveFloatProperty(HoldObjective, TEXT("ObjectiveProgress")), 0.5f);
	TestEqual(TEXT("GameState replicates half objective progress"), GetRiftObjectiveFloatProperty(RiftGameState, TEXT("ObjectiveProgress")), 0.5f);

	HoldObjective->Tick(15.1f);
	TestEqual(TEXT("Hold objective completes at full progress"), GetRiftObjectiveFloatProperty(HoldObjective, TEXT("ObjectiveProgress")), 1.0f);
	TestEqual(TEXT("Objective completes after hold duration"), GetRiftObjectiveEnumProperty(HoldObjective, TEXT("ObjectiveState")), static_cast<uint8>(EPRRiftObjectiveState::Completed));
	TestEqual(TEXT("GameState objective completes"), static_cast<uint8>(RiftGameState->GetCurrentObjectiveState()), static_cast<uint8>(EPRRiftObjectiveState::Completed));
	TestTrue(TEXT("Completed objective opens extraction"), RiftGameState->IsExtractionAvailable());
	FText CompletedPromptText;
	TestTrue(
		TEXT("Completed objective prompt changes to a completed message"),
		CallTextFunctionNoParams(HoldObjective, TEXT("GetInteractionPromptText"), CompletedPromptText)
		&& CompletedPromptText.ToString().Contains(TEXT("\u4EFB\u52A1\u5B8C\u6210")));

	return true;
}

#endif
