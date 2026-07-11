#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRExtractionZone.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTypes.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "Tests/PRProjectSettingsTestUtils.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRRiftExtractionZoneTest, "ProjectRift.Rift.ExtractionZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* LoadExtractionTestClass(const TCHAR* ClassName, UClass* ExpectedBaseClass)
{
	const FString ClassPath = FString::Printf(TEXT("/Script/ProjectA.%s"), ClassName);
	return StaticLoadClass(ExpectedBaseClass, nullptr, *ClassPath);
}

bool TestExtractionReplicatedProperty(FAutomationTestBase& Test, UClass* Class, const FName PropertyName)
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

bool CallExtractionBoolFunctionWithPawnParam(UObject* Target, const FName FunctionName, const FName ParamName, APawn* ParamValue, bool& bOutResult)
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

bool CallExtractionBoolFunctionNoParams(UObject* Target, const FName FunctionName, bool& bOutResult)
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

bool CallExtractionVoidFunctionWithBoolParam(UObject* Target, const FName FunctionName, const FName ParamName, const bool bParamValue)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	if (FBoolProperty* BoolParam = FindFProperty<FBoolProperty>(Function, ParamName))
	{
		BoolParam->SetPropertyValue_InContainer(ParamsMemory, bParamValue);
	}

	Target->ProcessEvent(Function, ParamsMemory);
	return true;
}

bool CallExtractionStringFunctionNoParams(UObject* Target, const FName FunctionName, FString& OutString)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FStrProperty* ReturnValue = FindFProperty<FStrProperty>(Function, TEXT("ReturnValue")))
	{
		OutString = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallExtractionTextFunctionNoParams(UObject* Target, const FName FunctionName, FText& OutText)
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

int32 GetExtractionIntPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FIntProperty* Property = Target ? FindFProperty<FIntProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetPropertyValue_InContainer(Target) : 0;
}

bool GetExtractionBoolPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FBoolProperty* Property = Target ? FindFProperty<FBoolProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property && Property->GetPropertyValue_InContainer(Target);
}

FPRItemInstance MakeExtractionSettlementItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}
}

bool FPRRiftExtractionZoneTest::RunTest(const FString& Parameters)
{
	ProjectRiftTests::FScopedProjectSettingsOverride SettingsOverride;
	SettingsOverride->ExtractionRadius = 410.0f;

	UClass* ExtractionZoneClass = LoadExtractionTestClass(TEXT("PRExtractionZone"), AActor::StaticClass());
	UClass* RiftGameModeClass = APRRiftGameMode::StaticClass();
	UClass* RiftGameStateClass = APRRiftGameState::StaticClass();

	TestNotNull(TEXT("APRExtractionZone runtime class exists"), ExtractionZoneClass);
	if (!ExtractionZoneClass)
	{
		return false;
	}

	TestTrue(TEXT("APRExtractionZone derives from AActor"), ExtractionZoneClass->IsChildOf(AActor::StaticClass()));
	TestNotNull(TEXT("Extraction zone exposes CanExtractPawn"), ExtractionZoneClass->FindFunctionByName(TEXT("CanExtractPawn")));
	TestNotNull(TEXT("Extraction zone exposes TryExtractPawn"), ExtractionZoneClass->FindFunctionByName(TEXT("TryExtractPawn")));
	TestNotNull(TEXT("Extraction zone exposes GetExtractionRadius"), ExtractionZoneClass->FindFunctionByName(TEXT("GetExtractionRadius")));
	TestNotNull(TEXT("Extraction zone exposes GetInteractionPromptText"), ExtractionZoneClass->FindFunctionByName(TEXT("GetInteractionPromptText")));
	TestNull(TEXT("Extraction radius is no longer an ExtractionZone Actor property"), ExtractionZoneClass->FindPropertyByName(TEXT("ExtractionRadius")));

	TestNotNull(TEXT("Rift GameMode exposes RegisterPlayerExtracted"), RiftGameModeClass->FindFunctionByName(TEXT("RegisterPlayerExtracted")));
	TestNotNull(TEXT("Rift GameMode exposes IsPlayerExtracted"), RiftGameModeClass->FindFunctionByName(TEXT("IsPlayerExtracted")));
	TestNotNull(TEXT("Rift GameMode exposes ResetExtractionState"), RiftGameModeClass->FindFunctionByName(TEXT("ResetExtractionState")));
	TestNotNull(TEXT("Rift GameMode exposes CheckExtractionCompletion"), RiftGameModeClass->FindFunctionByName(TEXT("CheckExtractionCompletion")));
	TestNotNull(TEXT("Rift GameMode exposes GetReturnToLobbyMapPath"), RiftGameModeClass->FindFunctionByName(TEXT("GetReturnToLobbyMapPath")));
	TestNotNull(TEXT("Rift GameMode exposes BuildReturnToLobbyTravelURL"), RiftGameModeClass->FindFunctionByName(TEXT("BuildReturnToLobbyTravelURL")));
	TestNotNull(TEXT("Rift GameMode exposes IsReturnToLobbyTravelPending"), RiftGameModeClass->FindFunctionByName(TEXT("IsReturnToLobbyTravelPending")));
	TestNotNull(TEXT("Rift GameMode exposes GetReturnToLobbyDelayAfterSettlement"), RiftGameModeClass->FindFunctionByName(TEXT("GetReturnToLobbyDelayAfterSettlement")));
	TestNotNull(TEXT("Rift GameMode exposes SetReturnToLobbyServerTravelEnabled"), RiftGameModeClass->FindFunctionByName(TEXT("SetReturnToLobbyServerTravelEnabled")));
	TestNotNull(TEXT("Rift GameMode exposes GenerateSettlementData"), RiftGameModeClass->FindFunctionByName(TEXT("GenerateSettlementData")));
	TestNotNull(TEXT("Rift GameMode exposes FinalizeRiftSettlement"), RiftGameModeClass->FindFunctionByName(TEXT("FinalizeRiftSettlement")));

	TestExtractionReplicatedProperty(*this, RiftGameStateClass, TEXT("ExtractedPlayerCount"));
	TestExtractionReplicatedProperty(*this, RiftGameStateClass, TEXT("bExtractionComplete"));
	TestExtractionReplicatedProperty(*this, RiftGameStateClass, TEXT("SettlementData"));
	TestExtractionReplicatedProperty(*this, RiftGameStateClass, TEXT("bSettlementReady"));
	TestNotNull(TEXT("Rift GameState exposes SetExtractedPlayerCount"), RiftGameStateClass->FindFunctionByName(TEXT("SetExtractedPlayerCount")));
	TestNotNull(TEXT("Rift GameState exposes SetExtractionComplete"), RiftGameStateClass->FindFunctionByName(TEXT("SetExtractionComplete")));
	TestNotNull(TEXT("Rift GameState exposes GetSettlementData"), RiftGameStateClass->FindFunctionByName(TEXT("GetSettlementData")));
	TestNotNull(TEXT("Rift GameState exposes IsSettlementReady"), RiftGameStateClass->FindFunctionByName(TEXT("IsSettlementReady")));
	TestNotNull(TEXT("Rift GameState exposes ResetSettlementData"), RiftGameStateClass->FindFunctionByName(TEXT("ResetSettlementData")));

	const AActor* ExtractionZoneDefaults = Cast<AActor>(ExtractionZoneClass->GetDefaultObject());
	TestNotNull(TEXT("APRExtractionZone has a default object"), ExtractionZoneDefaults);
	TestTrue(TEXT("Extraction zone replicates by default"), ExtractionZoneDefaults && ExtractionZoneDefaults->GetIsReplicated());

	const USphereComponent* ExtractionSphere = ExtractionZoneDefaults
		? FindObject<USphereComponent>(const_cast<AActor*>(ExtractionZoneDefaults), TEXT("ExtractionSphere"))
		: nullptr;
	const UStaticMeshComponent* ZoneMarkerMesh = ExtractionZoneDefaults
		? FindObject<UStaticMeshComponent>(const_cast<AActor*>(ExtractionZoneDefaults), TEXT("ZoneMarkerMesh"))
		: nullptr;
	const UWidgetComponent* PromptWidget = ExtractionZoneDefaults
		? FindObject<UWidgetComponent>(const_cast<AActor*>(ExtractionZoneDefaults), TEXT("InteractionPromptWidget"))
		: nullptr;

	TestNotNull(TEXT("Extraction zone owns an ExtractionSphere"), ExtractionSphere);
	TestNotNull(TEXT("Extraction zone owns a visible marker mesh"), ZoneMarkerMesh);
	TestNotNull(TEXT("Extraction zone owns a screen-space prompt widget"), PromptWidget);
	TestTrue(TEXT("Extraction sphere has a usable radius"), ExtractionSphere && ExtractionSphere->GetUnscaledSphereRadius() >= 240.0f);
	TestEqual(
		TEXT("Extraction sphere overlaps pawns"),
		ExtractionSphere ? ExtractionSphere->GetCollisionResponseToChannel(ECC_Pawn) : ECR_Ignore,
		ECR_Overlap);
	TestTrue(TEXT("Extraction prompt starts hidden"), PromptWidget && PromptWidget->bHiddenInGame);
	TestEqual(
		TEXT("Extraction prompt uses screen-space rendering"),
		PromptWidget ? PromptWidget->GetWidgetSpace() : EWidgetSpace::World,
		EWidgetSpace::Screen);
	TestTrue(
		TEXT("Extraction prompt uses a native widget class"),
		PromptWidget && PromptWidget->GetWidgetClass() != nullptr);

	FText DefaultPromptText;
	TestTrue(
		TEXT("Extraction prompt contains localized extraction copy"),
		CallExtractionTextFunctionNoParams(const_cast<AActor*>(ExtractionZoneDefaults), TEXT("GetInteractionPromptText"), DefaultPromptText)
		&& DefaultPromptText.ToString().Contains(TEXT("\u64A4\u79BB")));

	UClass* ExtractionZoneBlueprintClass = StaticLoadClass(
		AActor::StaticClass(),
		nullptr,
		TEXT("/Game/ProjectRift/Blueprints/Rift/BP_RiftExtractionZone.BP_RiftExtractionZone_C"));
	TestNotNull(TEXT("BP_RiftExtractionZone blueprint class loads"), ExtractionZoneBlueprintClass);
	TestTrue(
		TEXT("BP_RiftExtractionZone derives from APRExtractionZone"),
		ExtractionZoneBlueprintClass && ExtractionZoneBlueprintClass->IsChildOf(ExtractionZoneClass));

	UWorld* RiftTestWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/ProjectRift/Maps/L_Rift_Test.L_Rift_Test"));
	TestNotNull(TEXT("L_Rift_Test map asset loads for extraction zone check"), RiftTestWorld);
	bool bMapContainsExtractionZone = false;
	if (RiftTestWorld && RiftTestWorld->PersistentLevel)
	{
		for (AActor* Actor : RiftTestWorld->PersistentLevel->Actors)
		{
			if (Actor && Actor->GetClass()->IsChildOf(ExtractionZoneClass))
			{
				bMapContainsExtractionZone = true;
				break;
			}
		}
	}
	TestTrue(TEXT("L_Rift_Test contains a placed extraction zone"), bMapContainsExtractionZone);

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Extraction zone test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Extraction zone test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>();
	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	AActor* ExtractionZone = World->SpawnActor<AActor>(ExtractionZoneClass, FVector::ZeroVector, FRotator::ZeroRotator);
	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	ADefaultPawn* PlayerPawn = World->SpawnActor<ADefaultPawn>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);

	TestNotNull(TEXT("Extraction test world owns APRRiftGameMode"), RiftGameMode);
	TestNotNull(TEXT("Extraction test world owns APRRiftGameState"), RiftGameState);
	TestNotNull(TEXT("Can spawn extraction zone"), ExtractionZone);
	TestNotNull(TEXT("Can spawn extraction test player controller"), PlayerController);
	TestNotNull(TEXT("Can spawn extraction test player state"), PlayerState);
	TestNotNull(TEXT("Can spawn extraction test pawn"), PlayerPawn);
	if (!RiftGameMode || !RiftGameState || !ExtractionZone || !PlayerController || !PlayerState || !PlayerPawn)
	{
		return false;
	}
	APRExtractionZone* TypedExtractionZone = Cast<APRExtractionZone>(ExtractionZone);
	const USphereComponent* RuntimeExtractionSphere = FindObject<USphereComponent>(ExtractionZone, TEXT("ExtractionSphere"));
	TestEqual(TEXT("Extraction getter uses project settings radius"), TypedExtractionZone ? TypedExtractionZone->GetExtractionRadius() : 0.0f, 410.0f);
	TestEqual(TEXT("Extraction sphere uses project settings radius"), RuntimeExtractionSphere ? RuntimeExtractionSphere->GetUnscaledSphereRadius() : 0.0f, 410.0f);
	SettingsOverride->ExtractionRadius = -20.0f;
	if (TypedExtractionZone)
	{
		TypedExtractionZone->OnConstruction(TypedExtractionZone->GetActorTransform());
	}
	TestEqual(TEXT("Extraction radius clamps to one"), TypedExtractionZone ? TypedExtractionZone->GetExtractionRadius() : 0.0f, 1.0f);
	TestEqual(TEXT("Extraction sphere applies the clamped radius"), RuntimeExtractionSphere ? RuntimeExtractionSphere->GetUnscaledSphereRadius() : 0.0f, 1.0f);
	SettingsOverride->ExtractionRadius = 410.0f;
	if (TypedExtractionZone)
	{
		TypedExtractionZone->OnConstruction(TypedExtractionZone->GetActorTransform());
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(PlayerPawn);
	if (!RiftGameState->PlayerArray.Contains(PlayerState))
	{
		RiftGameState->AddPlayerState(PlayerState);
	}
	RiftGameState->SetAlivePlayerCount(1);
	RiftGameState->SetMissionTime(87.5f);
	RiftGameState->SetRiftStability(72.0f);
	RiftGameState->SetObjectiveProgress(1.0f);

	UPRInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent();
	TestNotNull(TEXT("Extraction settlement player owns inventory"), InventoryComponent);
	if (InventoryComponent)
	{
		TestTrue(TEXT("Can add extracted EnergyCrystal stack"), InventoryComponent->AddItem(MakeExtractionSettlementItem(TEXT("EnergyCrystal"), 2)));
		TestTrue(TEXT("Can add extracted RiftShard stack"), InventoryComponent->AddItem(MakeExtractionSettlementItem(TEXT("RiftShard"), 1)));
	}

	FString ReturnToLobbyMapPath;
	TestTrue(
		TEXT("Can query the extraction return lobby map path"),
		CallExtractionStringFunctionNoParams(RiftGameMode, TEXT("GetReturnToLobbyMapPath"), ReturnToLobbyMapPath));
	TestEqual(
		TEXT("Extraction returns to the ship lobby map"),
		ReturnToLobbyMapPath,
		FString(TEXT("/Game/ProjectRift/Maps/L_ShipLobby")));

	FString ReturnToLobbyTravelURL;
	TestTrue(
		TEXT("Can build extraction return lobby travel URL"),
		CallExtractionStringFunctionNoParams(RiftGameMode, TEXT("BuildReturnToLobbyTravelURL"), ReturnToLobbyTravelURL));
	TestEqual(
		TEXT("Extraction return keeps the listen server open"),
		ReturnToLobbyTravelURL,
		FString(TEXT("/Game/ProjectRift/Maps/L_ShipLobby?listen")));

	TestTrue(
		TEXT("Can disable real ServerTravel for the extraction automation test"),
		CallExtractionVoidFunctionWithBoolParam(
			RiftGameMode,
			TEXT("SetReturnToLobbyServerTravelEnabled"),
			TEXT("bEnabled"),
			false));

	bool bExtractedWhileLocked = true;
	TestTrue(
		TEXT("Can call TryExtractPawn while extraction is locked"),
		CallExtractionBoolFunctionWithPawnParam(ExtractionZone, TEXT("TryExtractPawn"), TEXT("ExtractingPawn"), PlayerPawn, bExtractedWhileLocked));
	TestFalse(TEXT("Player cannot extract before extraction opens"), bExtractedWhileLocked);
	TestEqual(TEXT("No players are extracted while extraction is locked"), GetExtractionIntPropertyValue(RiftGameState, TEXT("ExtractedPlayerCount")), 0);

	RiftGameMode->OpenExtraction();

	UWidgetComponent* RuntimePromptWidget = FindObject<UWidgetComponent>(ExtractionZone, TEXT("InteractionPromptWidget"));
	if (RuntimePromptWidget)
	{
		RuntimePromptWidget->SetHiddenInGame(true);
		RuntimePromptWidget->SetVisibility(false, true);
	}
	ExtractionZone->Tick(0.0f);
	TestFalse(
		TEXT("Extraction prompt becomes visible when a pawn is nearby"),
		RuntimePromptWidget ? RuntimePromptWidget->bHiddenInGame : true);
	TestEqual(TEXT("An already-overlapping player extracts when extraction opens"), GetExtractionIntPropertyValue(RiftGameState, TEXT("ExtractedPlayerCount")), 1);
	TestTrue(TEXT("All alive players extracted completes extraction"), GetExtractionBoolPropertyValue(RiftGameState, TEXT("bExtractionComplete")));
	TestTrue(TEXT("All alive players extracted marks settlement ready"), RiftGameState->IsSettlementReady());
	const FPRRiftSettlementData SettlementData = RiftGameState->GetSettlementData();
	TestEqual(TEXT("Extraction settlement records success"), SettlementData.Result, EPRRiftMissionResult::Success);
	TestEqual(TEXT("Extraction settlement records mission time"), SettlementData.MissionTime, 87.5f);
	TestEqual(TEXT("Extraction settlement records rift stability"), SettlementData.RiftStability, 72.0f);
	TestEqual(TEXT("Extraction settlement records objective progress"), SettlementData.ObjectiveProgress, 1.0f);
	TestEqual(TEXT("Extraction settlement records extracted players"), SettlementData.ExtractedPlayerCount, 1);
	TestEqual(TEXT("Extraction settlement records alive players"), SettlementData.AlivePlayerCount, 1);
	TestEqual(TEXT("Extraction settlement records carried item count"), SettlementData.ExtractedItemCount, 3);
	TestEqual(TEXT("Extraction settlement records carried item types"), SettlementData.ExtractedUniqueItemTypes, 2);
	bool bReturnToLobbyTravelPending = false;
	TestTrue(
		TEXT("Can query whether extraction requested lobby travel"),
		CallExtractionBoolFunctionNoParams(RiftGameMode, TEXT("IsReturnToLobbyTravelPending"), bReturnToLobbyTravelPending));
	TestTrue(TEXT("All alive players extracted requests travel back to the ship lobby"), bReturnToLobbyTravelPending);

	bool bDuplicateExtracted = true;
	TestTrue(
		TEXT("Can call TryExtractPawn for a duplicate extraction"),
		CallExtractionBoolFunctionWithPawnParam(ExtractionZone, TEXT("TryExtractPawn"), TEXT("ExtractingPawn"), PlayerPawn, bDuplicateExtracted));
	TestFalse(TEXT("Duplicate extraction is ignored"), bDuplicateExtracted);
	TestEqual(TEXT("Duplicate extraction does not increase count"), GetExtractionIntPropertyValue(RiftGameState, TEXT("ExtractedPlayerCount")), 1);

	return true;
}

#endif
