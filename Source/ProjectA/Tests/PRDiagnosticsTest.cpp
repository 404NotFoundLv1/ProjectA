#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Diagnostics/PRDiagnosticsSubsystem.h"
#include "Diagnostics/PRDeveloperToolsSubsystem.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameState.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Items/PRInventoryComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "UI/PRDiagnosticsWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDiagnosticsContractTest,
	"ProjectRift.Diagnostics.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDiagnosticsContractTest::RunTest(const FString& Parameters)
{
	const UEnum* SeverityEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRDiagnosticSeverity"));
	TestNotNull(TEXT("Diagnostic severity enum is registered"), SeverityEnum);

	for (const TCHAR* StructPath : {
		TEXT("/Script/ProjectA.PRDiagnosticContext"),
		TEXT("/Script/ProjectA.PRDiagnosticEvent"),
		TEXT("/Script/ProjectA.PRDiagnosticFilter"),
		TEXT("/Script/ProjectA.PRDiagnosticSnapshot"),
		TEXT("/Script/ProjectA.PRDiagnosticExportResult"),
		TEXT("/Script/ProjectA.PRDeveloperActionRequest"),
		TEXT("/Script/ProjectA.PRDeveloperActionAvailability"),
		TEXT("/Script/ProjectA.PRDeveloperActionResult") })
	{
		TestNotNull(*FString::Printf(TEXT("%s is registered"), StructPath), FindObject<UScriptStruct>(nullptr, StructPath));
	}

	const UClass* DiagnosticsClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRDiagnosticsSubsystem"));
	TestNotNull(TEXT("Diagnostics GameInstance subsystem is registered"), DiagnosticsClass);
	if (DiagnosticsClass)
	{
		for (const FName FunctionName : {
			FName(TEXT("RecordEvent")),
			FName(TEXT("GetEvents")),
			FName(TEXT("ClearEvents")),
			FName(TEXT("BuildSnapshot")),
			FName(TEXT("ExportSnapshot")) })
		{
			TestNotNull(*FString::Printf(TEXT("Diagnostics exposes %s"), *FunctionName.ToString()), DiagnosticsClass->FindFunctionByName(FunctionName));
		}
	}

	const UClass* ToolsClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRDeveloperToolsSubsystem"));
	TestNotNull(TEXT("Developer tools GameInstance subsystem is registered"), ToolsClass);
	if (ToolsClass)
	{
		TestNotNull(TEXT("Developer tools expose action availability"), ToolsClass->FindFunctionByName(TEXT("GetActionAvailability")));
		TestNotNull(TEXT("Developer tools expose action execution"), ToolsClass->FindFunctionByName(TEXT("ExecuteAction")));
	}

	const UClass* WidgetClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRDiagnosticsWidget"));
	TestNotNull(TEXT("Unified native diagnostics widget is registered"), WidgetClass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDiagnosticsBufferTest,
	"ProjectRift.Diagnostics.Buffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDiagnosticsBufferTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRDiagnosticsSubsystem* Diagnostics = NewObject<UPRDiagnosticsSubsystem>(GameInstance);
	FPRDiagnosticContext Context;
	Context.MapName = TEXT("L_Test");
	Context.NetMode = TEXT("Standalone");
	Context.ProfileId = FGuid::NewGuid();

	TestFalse(
		TEXT("An event without a category is rejected"),
		Diagnostics->RecordEvent(EPRDiagnosticSeverity::Info, NAME_None, TEXT("Invalid"), TEXT("message"), Context));
	TestFalse(
		TEXT("An event without a stable event code is rejected"),
		Diagnostics->RecordEvent(EPRDiagnosticSeverity::Info, TEXT("Save"), NAME_None, TEXT("message"), Context));
	TestFalse(
		TEXT("An event without a message is rejected"),
		Diagnostics->RecordEvent(EPRDiagnosticSeverity::Info, TEXT("Save"), TEXT("Invalid"), FString(), Context));

	bool bRecordedEveryValidEvent = true;
	for (int32 Index = 0; Index < 505; ++Index)
	{
		const EPRDiagnosticSeverity Severity = Index == 504 ? EPRDiagnosticSeverity::Error : EPRDiagnosticSeverity::Info;
		const FName Category = Index % 2 == 0 ? FName(TEXT("Save")) : FName(TEXT("Flow"));
		bRecordedEveryValidEvent &= Diagnostics->RecordEvent(
				Severity,
				Category,
				FName(*FString::Printf(TEXT("Event.%d"), Index)),
				Index == 504 ? FString::ChrN(1100, TEXT('X')) : FString::Printf(TEXT("event message %d"), Index),
				Context);
	}
	TestTrue(TEXT("Every valid event is recorded"), bRecordedEveryValidEvent);

	FPRDiagnosticFilter AllFilter;
	AllFilter.MinimumSeverity = EPRDiagnosticSeverity::Verbose;
	const TArray<FPRDiagnosticEvent> AllEvents = Diagnostics->GetEvents(AllFilter);
	TestEqual(TEXT("The default ring retains exactly 500 events"), AllEvents.Num(), 500);
	int64 LastSequence = 0;
	if (AllEvents.Num() == 500)
	{
		TestEqual(TEXT("The oldest retained event follows the five trimmed entries"), AllEvents[0].EventCode, FName(TEXT("Event.5")));
		TestEqual(TEXT("Events remain in oldest-to-newest order"), AllEvents.Last().EventCode, FName(TEXT("Event.504")));
		TestEqual(TEXT("Messages are limited to 1024 characters"), AllEvents.Last().Message.Len(), 1024);
		TestTrue(TEXT("Recorded events have monotonic sequence numbers"), AllEvents[0].Sequence < AllEvents.Last().Sequence);
		TestTrue(TEXT("Recorded events receive a UTC timestamp"), AllEvents.Last().TimestampUtc.GetTicks() > 0);
		LastSequence = AllEvents.Last().Sequence;
	}

	FPRDiagnosticFilter ErrorFilter;
	ErrorFilter.MinimumSeverity = EPRDiagnosticSeverity::Error;
	ErrorFilter.Categories = { TEXT("Save") };
	ErrorFilter.SearchText = TEXT("Event.504");
	const TArray<FPRDiagnosticEvent> ErrorEvents = Diagnostics->GetEvents(ErrorFilter);
	TestEqual(TEXT("Severity, category and text filters combine"), ErrorEvents.Num(), 1);

	Diagnostics->ClearEvents();
	TestEqual(TEXT("Clear removes every buffered event"), Diagnostics->GetEvents(AllFilter).Num(), 0);
	TestTrue(
		TEXT("Recording continues after clear"),
		Diagnostics->RecordEvent(EPRDiagnosticSeverity::Warning, TEXT("Diagnostics"), TEXT("AfterClear"), TEXT("after clear"), Context));
	const TArray<FPRDiagnosticEvent> AfterClear = Diagnostics->GetEvents(AllFilter);
	TestEqual(TEXT("One event exists after recording following clear"), AfterClear.Num(), 1);
	if (AfterClear.Num() == 1)
	{
		TestTrue(TEXT("Clear does not reset the process sequence"), AfterClear[0].Sequence > LastSequence);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDiagnosticsSnapshotTest,
	"ProjectRift.Diagnostics.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDiagnosticsSnapshotTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Diagnostics snapshot world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Diagnostics snapshot world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRGameState* GameState = World->SpawnActor<APRGameState>();
	World->SetGameState(GameState);
	APRPlayerController* Controller = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APRCharacter* Character = World->SpawnActor<APRCharacter>();
	TestNotNull(TEXT("Diagnostics snapshot controller exists"), Controller);
	TestNotNull(TEXT("Diagnostics snapshot PlayerState exists"), PlayerState);
	TestNotNull(TEXT("Diagnostics snapshot character exists"), Character);
	if (!Controller || !PlayerState || !Character || !GameState)
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	Controller->Possess(Character);
	GameState->SetTeamMissionState(TEXT("Mission.Rift.Test.Hold"), true);
	PlayerState->SetPlayerDisplayName(TEXT("Diagnostic Pilot"));
	PlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));

	FPRMultiplayerProfileProjection Projection;
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Diagnostic Profile");
	Projection.SelectedRoleId = TEXT("Ability.Role.Assault");
	Projection.Story.UnlockedChapterIds = { TEXT("Chapter.Prologue") };
	Projection.ShipModules = { FPRProfileShipModuleState{ TEXT("Ship.Module.Engine"), 1, false } };
	FString BindDiagnostic;
	TestTrue(TEXT("Diagnostics snapshot profile projection binds"), PlayerState->BindMultiplayerProfile(Projection, BindDiagnostic));
	PlayerState->AddShipResource(TEXT("EnergyCrystal"), 12);
	PlayerState->SetReady(true);

	UPRAttributeSet* Attributes = PlayerState->GetAttributeSet();
	TestNotNull(TEXT("Diagnostics snapshot attributes exist"), Attributes);
	if (Attributes)
	{
		Attributes->SetHealth(75.0f);
		Attributes->SetMaxHealth(100.0f);
		Attributes->SetShield(20.0f);
		Attributes->SetMaxShield(50.0f);
		Attributes->SetEnergy(40.0f);
		Attributes->SetMaxEnergy(100.0f);
	}

	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRDiagnosticsSubsystem* Diagnostics = NewObject<UPRDiagnosticsSubsystem>(GameInstance);
	const FPRDiagnosticSnapshot Snapshot = Diagnostics->BuildSnapshot(Controller);
	TestTrue(TEXT("Snapshot receives a UTC generation time"), Snapshot.GeneratedAtUtc.GetTicks() > 0);
	TestEqual(TEXT("Snapshot reports v0.6.2"), Snapshot.Environment.ProjectVersion, FString(TEXT("0.6.2")));
	TestFalse(TEXT("Snapshot reports a map"), Snapshot.Environment.MapName.IsEmpty());
	TestEqual(TEXT("Snapshot reports the owning controller"), Snapshot.Player.ControllerName, GetNameSafe(Controller));
	TestEqual(TEXT("Snapshot reports the bound profile"), Snapshot.Player.BoundProfileId, Projection.ProfileId);
	TestEqual(TEXT("Snapshot reports health"), Snapshot.Player.Health, 75.0f);
	TestEqual(TEXT("Snapshot reports role"), Snapshot.Player.SelectedRoleId, FName(TEXT("Ability.Role.Assault")));
	TestEqual(TEXT("Snapshot reports resources"), Snapshot.Player.ResourceSummary, FString(TEXT("EnergyCrystal x12")));
	const FPRDiagnosticTeamPlayerSnapshot* MatchingTeamPlayer = Snapshot.Team.Players.FindByPredicate(
		[&Projection](const FPRDiagnosticTeamPlayerSnapshot& TeamPlayer)
		{
			return TeamPlayer.BoundProfileId == Projection.ProfileId;
		});
	TestNotNull(TEXT("Snapshot reports the bound player in the team"), MatchingTeamPlayer);
	if (MatchingTeamPlayer)
	{
		TestTrue(TEXT("Snapshot reports team player ready"), MatchingTeamPlayer->bReady);
	}
	TestEqual(TEXT("Snapshot reports the selected mission"), Snapshot.Team.SelectedMissionId, FName(TEXT("Mission.Rift.Test.Hold")));
	TestTrue(TEXT("Snapshot reports team mission readiness"), Snapshot.Team.bTeamMissionReady);
	WorldWrapper.ForwardErrorMessages(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDiagnosticsExportTest,
	"ProjectRift.Diagnostics.Export",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDiagnosticsExportTest::RunTest(const FString& Parameters)
{
	const FString TestRoot = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("Diagnostics"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);

	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRDiagnosticsSubsystem* Diagnostics = NewObject<UPRDiagnosticsSubsystem>(GameInstance);
	Diagnostics->InitializeForAutomation(TestRoot);
	FPRDiagnosticContext Context;
	Context.ProfileId = FGuid::NewGuid();
	TestTrue(
		TEXT("Export test event is recorded"),
		Diagnostics->RecordEvent(EPRDiagnosticSeverity::Warning, TEXT("Save"), TEXT("Save.Pending"), TEXT("pending save"), Context));

	const FPRDiagnosticExportResult Result = Diagnostics->ExportSnapshot(nullptr);
	TestTrue(TEXT("Diagnostics export succeeds"), Result.bSuccess);
	TestEqual(TEXT("Diagnostics export reports one event"), Result.EventCount, 1);
	TestTrue(TEXT("Diagnostics export path remains under the automation root"), FPaths::IsUnderDirectory(Result.OutputPath, TestRoot));
	TestTrue(TEXT("Diagnostics export creates the final JSON file"), IFileManager::Get().FileExists(*Result.OutputPath));
	TestFalse(TEXT("Diagnostics export leaves no adjacent temporary file"), IFileManager::Get().FileExists(*(Result.OutputPath + TEXT(".tmp"))));

	FString JsonText;
	TSharedPtr<FJsonObject> JsonObject;
	const bool bLoadedJson = FFileHelper::LoadFileToString(JsonText, *Result.OutputPath);
	const bool bParsedJson = bLoadedJson && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), JsonObject) && JsonObject.IsValid();
	TestTrue(TEXT("Diagnostics export is valid JSON"), bParsedJson);
	if (bParsedJson)
	{
		TestEqual(TEXT("Diagnostics export schema is version one"), JsonObject->GetIntegerField(TEXT("schemaVersion")), 1);
		TestTrue(TEXT("Diagnostics export contains a snapshot object"), JsonObject->HasTypedField<EJson::Object>(TEXT("snapshot")));
		TestEqual(TEXT("Diagnostics export contains one event"), JsonObject->GetArrayField(TEXT("events")).Num(), 1);
	}

	const FString BlockedRoot = FPaths::Combine(TestRoot, TEXT("blocked-root"));
	TestTrue(TEXT("Blocked export root is represented by a file"), FFileHelper::SaveStringToFile(TEXT("blocked"), *BlockedRoot));
	Diagnostics->InitializeForAutomation(BlockedRoot);
	const FPRDiagnosticExportResult FailedResult = Diagnostics->ExportSnapshot(nullptr);
	TestFalse(TEXT("Diagnostics export reports a disk failure"), FailedResult.bSuccess);
	FPRDiagnosticFilter AllFilter;
	AllFilter.MinimumSeverity = EPRDiagnosticSeverity::Verbose;
	TestEqual(TEXT("Failed export does not clear buffered events"), Diagnostics->GetEvents(AllFilter).Num(), 1);

	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDeveloperToolsSafetyTest,
	"ProjectRift.Diagnostics.DeveloperToolsSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDeveloperToolsSafetyTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UPRDeveloperToolsSubsystem* DeveloperTools = NewObject<UPRDeveloperToolsSubsystem>(GameInstance);
	for (const EPRDeveloperAction DestructiveAction : {
		EPRDeveloperAction::DeleteProfile,
		EPRDeveloperAction::CorruptActivePrimary,
		EPRDeveloperAction::FailNextSave })
	{
		const FPRDeveloperActionAvailability Availability = DeveloperTools->GetActionAvailability(DestructiveAction, nullptr);
		TestTrue(TEXT("Destructive developer actions require explicit confirmation"), Availability.bRequiresConfirmation);
	}

	UPRSaveSubsystem* UninitializedSaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	TestFalse(TEXT("A normal save subsystem is not marked as an isolated development root"), UninitializedSaveSubsystem->IsUsingIsolatedDevelopmentRoot());
	TestEqual(
		TEXT("Corruption is rejected before any isolated development root is configured"),
		UninitializedSaveSubsystem->CorruptActivePrimaryForDevelopment().Status,
		EPRProfileOperationStatus::ValidationFailed);
	TestEqual(
		TEXT("Save failure injection is rejected before any isolated development root is configured"),
		UninitializedSaveSubsystem->FailNextSaveForDevelopment().Status,
		EPRProfileOperationStatus::ValidationFailed);

	const FString TestRoot = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Automation"),
		TEXT("DiagnosticsDeveloperTools"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	UPRSaveSubsystem* IsolatedSaveSubsystem = NewObject<UPRSaveSubsystem>(GameInstance);
	IsolatedSaveSubsystem->InitializeForAutomation(TestRoot);
	TestTrue(TEXT("Automation root is recognized as isolated"), IsolatedSaveSubsystem->IsUsingIsolatedDevelopmentRoot());
	TestTrue(TEXT("Isolated profile is created"), IsolatedSaveSubsystem->CreateProfile(TEXT("Diagnostics Safety")).IsSuccess());
	TestTrue(TEXT("A second isolated save creates a recovery backup"), IsolatedSaveSubsystem->SaveActiveProfile().IsSuccess());

	TestTrue(TEXT("Fail-next-save can be armed in an isolated root"), IsolatedSaveSubsystem->FailNextSaveForDevelopment().IsSuccess());
	TestEqual(
		TEXT("The armed isolated save fails exactly once"),
		IsolatedSaveSubsystem->SaveActiveProfile().Status,
		EPRProfileOperationStatus::IOError);
	TestTrue(TEXT("The following isolated save succeeds"), IsolatedSaveSubsystem->SaveActiveProfile().IsSuccess());
	TestTrue(TEXT("Primary corruption is available only for the isolated active profile"), IsolatedSaveSubsystem->CorruptActivePrimaryForDevelopment().IsSuccess());

	IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDiagnosticsUIContractTest,
	"ProjectRift.Diagnostics.UIContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDiagnosticsUIContractTest::RunTest(const FString& Parameters)
{
	const UEnum* TabEnum = StaticEnum<EPRDiagnosticsTab>();
	TestNotNull(TEXT("Diagnostics tab enum is registered"), TabEnum);
	if (TabEnum)
	{
		for (const TCHAR* TabName : { TEXT("Overview"), TEXT("Player"), TEXT("TeamAndRift"), TEXT("Events"), TEXT("Tools") })
		{
			TestTrue(
				FString::Printf(TEXT("Diagnostics exposes the %s tab"), TabName),
				TabEnum->GetIndexByNameString(TabName) != INDEX_NONE);
		}
	}

	UClass* WidgetClass = UPRDiagnosticsWidget::StaticClass();
	TestNotNull(TEXT("Diagnostics widget initializes for a local controller"), WidgetClass->FindFunctionByName(TEXT("InitializeForController")));
	TestNotNull(TEXT("Diagnostics widget exposes tab selection"), WidgetClass->FindFunctionByName(TEXT("SetActiveTab")));
	TestNotNull(TEXT("Diagnostics widget exposes the active tab"), WidgetClass->FindFunctionByName(TEXT("GetActiveTab")));

	UClass* ControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Player controller exposes the F1 diagnostics toggle"), ControllerClass->FindFunctionByName(TEXT("ToggleDiagnosticsPanel")));
	TestNotNull(TEXT("Player controller exposes diagnostics close"), ControllerClass->FindFunctionByName(TEXT("HideDiagnosticsPanel")));
	TestNotNull(TEXT("Player controller owns a configurable diagnostics widget class"), FindFProperty<FClassProperty>(ControllerClass, TEXT("DiagnosticsWidgetClass")));
	return true;
}

#endif
