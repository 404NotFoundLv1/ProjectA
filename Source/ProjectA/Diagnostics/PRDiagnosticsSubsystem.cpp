#include "Diagnostics/PRDiagnosticsSubsystem.h"

#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameInstance.h"
#include "Core/PRGameState.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Items/PRInventoryComponent.h"
#include "JsonObjectConverter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Settings/PRProjectSettings.h"

namespace
{
constexpr int32 DiagnosticsExportSchemaVersion = 1;

FString GetBuildConfigurationName()
{
#if UE_BUILD_DEBUG
	return TEXT("Debug");
#elif UE_BUILD_DEVELOPMENT
	return TEXT("Development");
#elif UE_BUILD_TEST
	return TEXT("Test");
#elif UE_BUILD_SHIPPING
	return TEXT("Shipping");
#else
	return TEXT("Unknown");
#endif
}

FString GetWorldTypeName(const EWorldType::Type WorldType)
{
	switch (WorldType)
	{
	case EWorldType::Game: return TEXT("Game");
	case EWorldType::PIE: return TEXT("PIE");
	case EWorldType::Editor: return TEXT("Editor");
	case EWorldType::EditorPreview: return TEXT("EditorPreview");
	case EWorldType::GamePreview: return TEXT("GamePreview");
	case EWorldType::GameRPC: return TEXT("GameRPC");
	case EWorldType::Inactive: return TEXT("Inactive");
	case EWorldType::None: return TEXT("None");
	default: return TEXT("Unknown");
	}
}

FString GetNetModeName(const ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone: return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer: return TEXT("ListenServer");
	case NM_Client: return TEXT("Client");
	default: return TEXT("Unknown");
	}
}

FString BuildInventorySummary(const UPRInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return TEXT("Unavailable");
	}

	TArray<FPRItemInstance> Items = Inventory->GetInventoryItems();
	Items.RemoveAll([](const FPRItemInstance& Item) { return !Item.IsValid(); });
	Items.Sort([](const FPRItemInstance& Left, const FPRItemInstance& Right)
	{
		return Left.ItemId.LexicalLess(Right.ItemId);
	});
	if (Items.IsEmpty())
	{
		return TEXT("None");
	}

	TArray<FString> Parts;
	Parts.Reserve(Items.Num());
	for (const FPRItemInstance& Item : Items)
	{
		Parts.Add(FString::Printf(TEXT("%s x%d"), *Item.ItemId.ToString(), Item.Count));
	}
	return FString::Join(Parts, TEXT(", "));
}

FString BuildShipModuleSummary(const TArray<FPRProfileShipModuleState>& InModules)
{
	TArray<FPRProfileShipModuleState> Modules = InModules;
	Modules.RemoveAll([](const FPRProfileShipModuleState& Module)
	{
		return Module.ModuleId.IsNone() || Module.Level < 0;
	});
	Modules.Sort([](const FPRProfileShipModuleState& Left, const FPRProfileShipModuleState& Right)
	{
		return Left.ModuleId.LexicalLess(Right.ModuleId);
	});
	if (Modules.IsEmpty())
	{
		return TEXT("None");
	}

	TArray<FString> Parts;
	Parts.Reserve(Modules.Num());
	for (const FPRProfileShipModuleState& Module : Modules)
	{
		Parts.Add(FString::Printf(TEXT("%s Lv%d"), *Module.ModuleId.ToString(), Module.Level));
	}
	return FString::Join(Parts, TEXT(", "));
}

FString SanitizeExportToken(FString Value)
{
	Value.TrimStartAndEndInline();
	for (TCHAR& Character : Value)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
		{
			Character = TEXT('_');
		}
	}
	return Value.IsEmpty() ? TEXT("Unknown") : Value.Left(64);
}

bool IsPathWithin(const FString& Candidate, const FString& Parent)
{
	FString FullCandidate = FPaths::ConvertRelativePathToFull(Candidate);
	FString FullParent = FPaths::ConvertRelativePathToFull(Parent);
	FPaths::NormalizeDirectoryName(FullCandidate);
	FPaths::NormalizeDirectoryName(FullParent);
	return FullCandidate == FullParent || FPaths::IsUnderDirectory(FullCandidate, FullParent);
}
}

bool UPRDiagnosticsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}

void UPRDiagnosticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	MaxEventCount = Settings ? FMath::Clamp(Settings->DiagnosticEventCapacity, 100, 5000) : 500;
}

bool UPRDiagnosticsSubsystem::RecordEvent(
	const EPRDiagnosticSeverity Severity,
	const FName Category,
	const FName EventCode,
	const FString& Message,
	const FPRDiagnosticContext& Context)
{
	if (Category.IsNone() || EventCode.IsNone())
	{
		return false;
	}

	FString NormalizedMessage = Message;
	NormalizedMessage.TrimStartAndEndInline();
	if (NormalizedMessage.IsEmpty())
	{
		return false;
	}
	NormalizedMessage.LeftInline(1024, EAllowShrinking::No);

	FScopeLock Lock(&EventMutex);
	FPRDiagnosticEvent& Event = Events.AddDefaulted_GetRef();
	Event.Sequence = NextSequence++;
	Event.TimestampUtc = FDateTime::UtcNow();
	Event.Severity = Severity;
	Event.Category = Category;
	Event.EventCode = EventCode;
	Event.Message = MoveTemp(NormalizedMessage);
	Event.Context = Context;

	const int32 ExcessCount = Events.Num() - FMath::Max(1, MaxEventCount);
	if (ExcessCount > 0)
	{
		Events.RemoveAt(0, ExcessCount, EAllowShrinking::No);
	}
	return true;
}

TArray<FPRDiagnosticEvent> UPRDiagnosticsSubsystem::GetEvents(const FPRDiagnosticFilter& Filter) const
{
	FScopeLock Lock(&EventMutex);
	TArray<FPRDiagnosticEvent> Result;
	Result.Reserve(Events.Num());
	for (const FPRDiagnosticEvent& Event : Events)
	{
		if (Filter.Matches(Event))
		{
			Result.Add(Event);
		}
	}
	return Result;
}

void UPRDiagnosticsSubsystem::ClearEvents()
{
	FScopeLock Lock(&EventMutex);
	Events.Reset();
}

FPRDiagnosticSnapshot UPRDiagnosticsSubsystem::BuildSnapshot(APlayerController* LocalController) const
{
	FPRDiagnosticSnapshot Snapshot;
	Snapshot.GeneratedAtUtc = FDateTime::UtcNow();
	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		Snapshot.Environment.ProjectVersion,
		GGameIni);
	Snapshot.Environment.EngineVersion = FEngineVersion::Current().ToString();
	Snapshot.Environment.BuildConfiguration = GetBuildConfigurationName();
	Snapshot.Environment.PlatformName = FPlatformProperties::PlatformName();
	Snapshot.Environment.UptimeSeconds = FPlatformTime::Seconds();

	UWorld* World = LocalController ? LocalController->GetWorld() : nullptr;
	Snapshot.Environment.MapName = World ? UWorld::RemovePIEPrefix(World->GetMapName()) : TEXT("NoWorld");
	Snapshot.Environment.WorldType = World ? GetWorldTypeName(World->WorldType) : TEXT("Unavailable");
	Snapshot.Environment.NetMode = World ? GetNetModeName(World->GetNetMode()) : TEXT("Standalone");
	if (World)
	{
		const float DeltaSeconds = World->GetDeltaSeconds();
		Snapshot.Environment.AverageFrameTimeMs = DeltaSeconds > 0.0f ? DeltaSeconds * 1000.0f : 0.0f;
		Snapshot.Environment.AverageFps = DeltaSeconds > 0.0f ? 1.0f / DeltaSeconds : 0.0f;
		if (GEngine)
		{
			if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
			{
				Snapshot.Environment.PIEInstance = WorldContext->PIEInstance;
			}
		}
	}

	APRPlayerState* LocalPlayerState = LocalController ? LocalController->GetPlayerState<APRPlayerState>() : nullptr;
	APRCharacter* LocalCharacter = LocalController ? Cast<APRCharacter>(LocalController->GetPawn()) : nullptr;
	Snapshot.Player.ControllerName = GetNameSafe(LocalController);
	Snapshot.Player.PawnName = GetNameSafe(LocalController ? LocalController->GetPawn() : nullptr);
	Snapshot.Player.PlayerStateName = GetNameSafe(LocalPlayerState);
	if (LocalPlayerState)
	{
		Snapshot.Player.BoundProfileId = LocalPlayerState->GetBoundProfileId();
		Snapshot.Player.SelectedRoleId = LocalPlayerState->GetSelectedRoleModule();
		Snapshot.Player.bPendingRepair = LocalPlayerState->IsRepairPersistencePending();
		Snapshot.Player.ResourceSummary = LocalPlayerState->BuildShipResourceSummary();
		Snapshot.Player.InventorySummary = BuildInventorySummary(LocalPlayerState->GetInventoryComponent());
		Snapshot.Player.CurrentChapterId = LocalPlayerState->GetBoundStoryProgress().CurrentChapterId;
		Snapshot.Player.ShipModuleSummary = BuildShipModuleSummary(LocalPlayerState->GetBoundShipModules());
		if (const UPRAttributeSet* Attributes = LocalPlayerState->GetAttributeSet())
		{
			Snapshot.Player.Health = Attributes->GetHealth();
			Snapshot.Player.MaxHealth = Attributes->GetMaxHealth();
			Snapshot.Player.Shield = Attributes->GetShield();
			Snapshot.Player.MaxShield = Attributes->GetMaxShield();
			Snapshot.Player.Energy = Attributes->GetEnergy();
			Snapshot.Player.MaxEnergy = Attributes->GetMaxEnergy();
		}
	}
	Snapshot.Player.bDowned = LocalCharacter && LocalCharacter->IsDowned();

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (GameInstance)
	{
		if (UPRSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UPRSaveSubsystem>())
		{
			Snapshot.Player.ActiveProfileId = SaveSubsystem->GetActiveProfileId();
			Snapshot.Player.ActiveProfileDisplayName = SaveSubsystem->GetActiveProfileDisplayName();
			Snapshot.Player.bPendingSettlement = SaveSubsystem->HasPendingSettlementReceipt();
			Snapshot.Player.bPendingRepair |= SaveSubsystem->HasPendingShipRepairReceipt();
			Snapshot.Player.LastSaveStatus = StaticEnum<EPRProfileOperationStatus>()->GetNameStringByValue(
				static_cast<int64>(SaveSubsystem->GetLastOperationResult().Status));

			FPRProfileSnapshot ActiveSnapshot;
			if (SaveSubsystem->GetActiveProfileSnapshot(ActiveSnapshot))
			{
				if (!LocalPlayerState || !LocalPlayerState->IsMultiplayerProfileBound())
				{
					Snapshot.Player.SelectedRoleId = ActiveSnapshot.SelectedRoleId;
					Snapshot.Player.CurrentChapterId = ActiveSnapshot.Story.CurrentChapterId;
					Snapshot.Player.ShipModuleSummary = BuildShipModuleSummary(ActiveSnapshot.ShipModules);
				}
			}
		}

		if (const UPRGameInstance* ProjectRiftGameInstance = Cast<UPRGameInstance>(GameInstance))
		{
			Snapshot.Team.SessionState = StaticEnum<EPRSessionInterfaceState>()->GetNameStringByValue(
				static_cast<int64>(ProjectRiftGameInstance->GetSessionInterfaceState()));
		}
	}
	if (Snapshot.Team.SessionState.IsEmpty())
	{
		Snapshot.Team.SessionState = TEXT("Unavailable");
	}

	if (World)
	{
		if (const APRGameState* GameState = World->GetGameState<APRGameState>())
		{
			Snapshot.Team.SelectedMissionId = GameState->GetSelectedTeamMissionId();
			Snapshot.Team.bTeamMissionReady = GameState->IsTeamMissionReady();
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
				if (!ProjectRiftPlayerState)
				{
					continue;
				}
				FPRDiagnosticTeamPlayerSnapshot& TeamPlayer = Snapshot.Team.Players.AddDefaulted_GetRef();
				TeamPlayer.PlayerId = ProjectRiftPlayerState->GetPlayerId();
				TeamPlayer.DisplayName = ProjectRiftPlayerState->GetPlayerDisplayName();
				TeamPlayer.BoundProfileId = ProjectRiftPlayerState->GetBoundProfileId();
				TeamPlayer.bProfileBound = ProjectRiftPlayerState->IsMultiplayerProfileBound();
				TeamPlayer.bReady = ProjectRiftPlayerState->IsReady();
				TeamPlayer.SelectedRoleId = ProjectRiftPlayerState->GetSelectedRoleModule();
				TeamPlayer.bRepairPending = ProjectRiftPlayerState->IsRepairPersistencePending();
			}
			Snapshot.Team.Players.Sort([](const FPRDiagnosticTeamPlayerSnapshot& Left, const FPRDiagnosticTeamPlayerSnapshot& Right)
			{
				return Left.PlayerId < Right.PlayerId;
			});
		}

		if (const APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>())
		{
			Snapshot.Rift.bAvailable = true;
			Snapshot.Rift.MissionId = RiftGameState->GetSelectedTeamMissionId();
			Snapshot.Rift.ObjectiveState = StaticEnum<EPRRiftObjectiveState>()->GetNameStringByValue(
				static_cast<int64>(RiftGameState->GetCurrentObjectiveState()));
			Snapshot.Rift.Stability = RiftGameState->GetRiftStability();
			Snapshot.Rift.MissionTime = RiftGameState->GetMissionTime();
			Snapshot.Rift.ObjectiveProgress = RiftGameState->GetObjectiveProgress();
			Snapshot.Rift.AlivePlayers = RiftGameState->GetAlivePlayerCount();
			Snapshot.Rift.ExtractedPlayers = RiftGameState->GetExtractedPlayerCount();
			Snapshot.Rift.KilledEnemies = RiftGameState->GetKilledEnemyCount();
			Snapshot.Rift.EncounterDirector = RiftGameState->GetEncounterDirectorSnapshot();
			Snapshot.Rift.bSettlementReady = RiftGameState->IsSettlementReady();
			const FPRRiftSettlementData Settlement = RiftGameState->GetSettlementData();
			Snapshot.Rift.RunId = Settlement.RunId;
			Snapshot.Rift.SettlementId = Settlement.SettlementId;
			Snapshot.Rift.SettlementStatus = StaticEnum<EPRRiftMissionResult>()->GetNameStringByValue(
				static_cast<int64>(Settlement.Result));
		}
		if (const APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>())
		{
			Snapshot.Rift.bAvailable = true;
			Snapshot.Rift.MissionId = RiftGameMode->GetMissionId();
			Snapshot.Rift.RunId = RiftGameMode->GetCurrentRunId();
			Snapshot.Rift.SettlementId = RiftGameMode->GetCurrentSettlementId();
		}
	}

	if (Snapshot.Team.Players.IsEmpty())
	{
		Snapshot.Team.StartBlockReason = TEXT("No ProjectRift players connected.");
	}
	else if (Snapshot.Team.Players.ContainsByPredicate([](const FPRDiagnosticTeamPlayerSnapshot& Player) { return !Player.bProfileBound; }))
	{
		Snapshot.Team.StartBlockReason = TEXT("A player has no bound profile.");
	}
	else if (Snapshot.Team.Players.ContainsByPredicate([](const FPRDiagnosticTeamPlayerSnapshot& Player) { return Player.bRepairPending; }))
	{
		Snapshot.Team.StartBlockReason = TEXT("A ship repair is waiting to be saved.");
	}
	else if (Snapshot.Team.Players.ContainsByPredicate([](const FPRDiagnosticTeamPlayerSnapshot& Player) { return !Player.bReady; }))
	{
		Snapshot.Team.StartBlockReason = TEXT("Not every player is ready.");
	}
	else if (Snapshot.Team.SelectedMissionId.IsNone())
	{
		Snapshot.Team.StartBlockReason = TEXT("No team mission is selected.");
	}
	else if (!Snapshot.Team.bTeamMissionReady)
	{
		Snapshot.Team.StartBlockReason = TEXT("The selected team mission is not eligible to start.");
	}

	FPRDiagnosticFilter EventFilter;
	EventFilter.MinimumSeverity = EPRDiagnosticSeverity::Verbose;
	const TArray<FPRDiagnosticEvent> CurrentEvents = GetEvents(EventFilter);
	if (CurrentEvents.ContainsByPredicate([](const FPRDiagnosticEvent& Event)
		{ return Event.Severity >= EPRDiagnosticSeverity::Error; }))
	{
		Snapshot.Health = EPRDiagnosticHealth::Error;
		Snapshot.HealthMessages.Add(TEXT("One or more diagnostic errors were recorded."));
	}
	else if (CurrentEvents.ContainsByPredicate([](const FPRDiagnosticEvent& Event)
		{ return Event.Severity == EPRDiagnosticSeverity::Warning; })
		|| Snapshot.Player.bPendingSettlement || Snapshot.Player.bPendingRepair)
	{
		Snapshot.Health = EPRDiagnosticHealth::Warning;
		Snapshot.HealthMessages.Add(TEXT("Warnings or pending persistence operations require attention."));
	}
	return Snapshot;
}

FPRDiagnosticExportResult UPRDiagnosticsSubsystem::ExportSnapshot(APlayerController* LocalController)
{
	FPRDiagnosticExportResult Result;
	const FString ProjectSavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	const FString DefaultRoot = FPaths::Combine(ProjectSavedRoot, TEXT("Diagnostics"));
	FString ExportRoot = ExportRootOverride.IsEmpty() ? DefaultRoot : ExportRootOverride;
	ExportRoot = FPaths::ConvertRelativePathToFull(ExportRoot);

	const FString AutomationRoot = FPaths::Combine(ProjectSavedRoot, TEXT("Automation"));
	const bool bAllowedRoot = ExportRootOverride.IsEmpty()
		? IsPathWithin(ExportRoot, DefaultRoot)
		: IsPathWithin(ExportRoot, AutomationRoot);
	if (!bAllowedRoot)
	{
		Result.Diagnostic = TEXT("Diagnostics export root is outside ProjectA's approved Saved directories.");
		return Result;
	}
	if (IFileManager::Get().FileExists(*ExportRoot))
	{
		Result.Diagnostic = TEXT("Diagnostics export root is occupied by a file.");
		return Result;
	}
	if (!IFileManager::Get().DirectoryExists(*ExportRoot)
		&& !IFileManager::Get().MakeDirectory(*ExportRoot, true))
	{
		Result.Diagnostic = TEXT("Unable to create the diagnostics export directory.");
		return Result;
	}

	FPRDiagnosticFilter AllEventsFilter;
	AllEventsFilter.MinimumSeverity = EPRDiagnosticSeverity::Verbose;
	const TArray<FPRDiagnosticEvent> ExportEvents = GetEvents(AllEventsFilter);
	const FPRDiagnosticSnapshot Snapshot = BuildSnapshot(LocalController);

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("schemaVersion"), DiagnosticsExportSchemaVersion);
	TSharedRef<FJsonObject> SnapshotObject = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		FPRDiagnosticSnapshot::StaticStruct(), &Snapshot, SnapshotObject, 0, 0))
	{
		Result.Diagnostic = TEXT("Unable to serialize the diagnostics snapshot.");
		return Result;
	}
	RootObject->SetObjectField(TEXT("snapshot"), SnapshotObject);

	TArray<TSharedPtr<FJsonValue>> EventValues;
	EventValues.Reserve(ExportEvents.Num());
	for (const FPRDiagnosticEvent& Event : ExportEvents)
	{
		TSharedRef<FJsonObject> EventObject = MakeShared<FJsonObject>();
		if (!FJsonObjectConverter::UStructToJsonObject(FPRDiagnosticEvent::StaticStruct(), &Event, EventObject, 0, 0))
		{
			Result.Diagnostic = TEXT("Unable to serialize a diagnostics event.");
			return Result;
		}
		EventValues.Add(MakeShared<FJsonValueObject>(EventObject));
	}
	RootObject->SetArrayField(TEXT("events"), EventValues);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		Result.Diagnostic = TEXT("Unable to encode diagnostics JSON.");
		return Result;
	}

	const FString Timestamp = Snapshot.GeneratedAtUtc.ToString(TEXT("%Y%m%d-%H%M%S-%s"));
	const FString FileName = FString::Printf(
		TEXT("ProjectRift-%s-%s-%s-%s.json"),
		*Timestamp,
		*SanitizeExportToken(Snapshot.Environment.MapName),
		*SanitizeExportToken(Snapshot.Environment.NetMode),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	const FString FinalPath = FPaths::Combine(ExportRoot, FileName);
	const FString TemporaryPath = FinalPath + TEXT(".tmp");
	if (!IsPathWithin(FinalPath, ExportRoot)
		|| !FFileHelper::SaveStringToFile(JsonText, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Result.Diagnostic = TEXT("Unable to write the temporary diagnostics export.");
		return Result;
	}

	FString VerifiedText;
	TSharedPtr<FJsonObject> VerifiedObject;
	const bool bVerified = FFileHelper::LoadFileToString(VerifiedText, *TemporaryPath)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VerifiedText), VerifiedObject)
		&& VerifiedObject.IsValid()
		&& VerifiedObject->HasTypedField<EJson::Object>(TEXT("snapshot"))
		&& VerifiedObject->HasTypedField<EJson::Array>(TEXT("events"));
	if (!bVerified)
	{
		IFileManager::Get().Delete(*TemporaryPath, false, true);
		Result.Diagnostic = TEXT("Diagnostics export failed readback validation.");
		return Result;
	}
	if (!IFileManager::Get().Move(*FinalPath, *TemporaryPath, true, true, false, false))
	{
		IFileManager::Get().Delete(*TemporaryPath, false, true);
		Result.Diagnostic = TEXT("Unable to promote the verified diagnostics export.");
		return Result;
	}

	Result.bSuccess = true;
	Result.OutputPath = FinalPath;
	Result.EventCount = ExportEvents.Num();
	Result.Diagnostic = TEXT("Diagnostics exported successfully.");
	return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
void UPRDiagnosticsSubsystem::InitializeForAutomation(const FString& InExportRoot)
{
	ExportRootOverride = InExportRoot;
}
#endif
