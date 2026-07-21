#include "UI/PRDiagnosticsWidget.h"

#include "Core/PRGameState.h"
#include "Diagnostics/PRDeveloperToolsSubsystem.h"
#include "Diagnostics/PRDiagnosticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FText TabLabel(const EPRDiagnosticsTab Tab)
{
	switch (Tab)
	{
	case EPRDiagnosticsTab::Overview: return FText::FromString(TEXT("Overview"));
	case EPRDiagnosticsTab::Player: return FText::FromString(TEXT("Player"));
	case EPRDiagnosticsTab::TeamAndRift: return FText::FromString(TEXT("Team & Rift"));
	case EPRDiagnosticsTab::Events: return FText::FromString(TEXT("Events"));
	case EPRDiagnosticsTab::Tools: return FText::FromString(TEXT("Tools"));
	default: return FText::FromString(TEXT("Diagnostics"));
	}
}

FString GuidOrNone(const FGuid& Guid)
{
	return Guid.IsValid() ? Guid.ToString(EGuidFormats::DigitsWithHyphensLower) : TEXT("None");
}
}

TSharedRef<SWidget> UPRDiagnosticsWidget::RebuildWidget()
{
	const TSharedRef<SWidget> Root = SNew(SBorder)
		.Padding(0.0f)
		.BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.82f))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(1120.0f)
				.HeightOverride(700.0f)
				.Padding(FMargin(18.0f))
				[
					SNew(SBorder)
					.Padding(18.0f)
					.BorderBackgroundColor(FLinearColor(0.035f, 0.05f, 0.075f, 0.98f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SAssignNew(TitleText, STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20))
								.ColorAndOpacity(FLinearColor(0.55f, 0.9f, 1.0f, 1.0f))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(FText::FromString(TEXT("Close [Esc]")))
								.OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleClose)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
							[SNew(SButton).Text(TabLabel(EPRDiagnosticsTab::Overview)).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleTabSelected, EPRDiagnosticsTab::Overview)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
							[SNew(SButton).Text(TabLabel(EPRDiagnosticsTab::Player)).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleTabSelected, EPRDiagnosticsTab::Player)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
							[SNew(SButton).Text(TabLabel(EPRDiagnosticsTab::TeamAndRift)).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleTabSelected, EPRDiagnosticsTab::TeamAndRift)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
							[SNew(SButton).Text(TabLabel(EPRDiagnosticsTab::Events)).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleTabSelected, EPRDiagnosticsTab::Events)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
							[SNew(SButton).Text(TabLabel(EPRDiagnosticsTab::Tools)).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleTabSelected, EPRDiagnosticsTab::Tools)]
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SNew(SBorder)
							.Padding(12.0f)
							.BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.035f, 0.95f))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()
									[
										SAssignNew(BodyText, STextBlock)
										.AutoWrapText(true)
										.ColorAndOpacity(FLinearColor::White)
									]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										SAssignNew(ToolsBox, SVerticalBox)
									]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
						[
							SAssignNew(StatusText, STextBlock)
							.AutoWrapText(true)
							.ColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.25f, 1.0f))
						]
					]
				]
			]
		];
	RefreshBody();
	RefreshTools();
	return Root;
}

void UPRDiagnosticsWidget::InitializeForController(APlayerController* InLocalController, const EPRDiagnosticsTab InitialTab)
{
	LocalController = InLocalController;
	SetActiveTab(InitialTab);
}

void UPRDiagnosticsWidget::SetActiveTab(const EPRDiagnosticsTab NewTab)
{
	ActiveTab = NewTab;
	ClearConfirmation();
	RefreshBody();
	RefreshTools();
}

void UPRDiagnosticsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccumulator += InDeltaTime;
	ToolsRefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.25)
	{
		RefreshAccumulator = 0.0;
		RefreshBody();
	}
	if (ActiveTab == EPRDiagnosticsTab::Tools && ToolsRefreshAccumulator >= 1.0)
	{
		ToolsRefreshAccumulator = 0.0;
		RefreshTools();
	}
}

FReply UPRDiagnosticsWidget::HandleClose()
{
	if (APRPlayerController* Controller = Cast<APRPlayerController>(LocalController.Get()))
	{
		Controller->HideDiagnosticsPanel();
	}
	return FReply::Handled();
}

FReply UPRDiagnosticsWidget::HandleTabSelected(const EPRDiagnosticsTab NewTab)
{
	SetActiveTab(NewTab);
	return FReply::Handled();
}

FReply UPRDiagnosticsWidget::HandleDeveloperAction(const EPRDeveloperAction Action, const FGuid ProfileId)
{
	APlayerController* Controller = LocalController.Get();
	UGameInstance* GameInstance = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameInstance() : nullptr;
	UPRDeveloperToolsSubsystem* DeveloperTools = GameInstance ? GameInstance->GetSubsystem<UPRDeveloperToolsSubsystem>() : nullptr;
	if (!DeveloperTools)
	{
		StatusMessage = TEXT("Developer tools are unavailable in this build or world.");
		RefreshBody();
		return FReply::Handled();
	}

	const FPRDeveloperActionAvailability Availability = DeveloperTools->GetActionAvailability(Action, Controller);
	if (!Availability.bAvailable)
	{
		StatusMessage = Availability.DisabledReason;
		RefreshBody();
		return FReply::Handled();
	}
	if (Availability.bRequiresConfirmation && !IsMatchingConfirmation(Action, ProfileId))
	{
		bConfirmationPending = true;
		PendingConfirmationAction = Action;
		PendingConfirmationProfileId = ProfileId;
		StatusMessage = TEXT("Destructive action armed. Click the same action again to confirm; changing tabs cancels it.");
		RefreshBody();
		return FReply::Handled();
	}

	FPRDeveloperActionRequest Request;
	Request.Action = Action;
	Request.ProfileId = ProfileId;
	Request.bConfirmed = Availability.bRequiresConfirmation;
	if (Action == EPRDeveloperAction::CreateProfile)
	{
		Request.TextValue = FString::Printf(TEXT("Developer Profile %s"), *FDateTime::Now().ToString(TEXT("%H%M%S")));
	}
	const FPRDeveloperActionResult Result = DeveloperTools->ExecuteAction(Request, Controller);
	StatusMessage = Result.Diagnostic;
	ClearConfirmation();
	RefreshBody();
	RefreshTools();
	return FReply::Handled();
}

FReply UPRDiagnosticsWidget::HandleExport()
{
	APlayerController* Controller = LocalController.Get();
	UGameInstance* GameInstance = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameInstance() : nullptr;
	UPRDiagnosticsSubsystem* Diagnostics = GameInstance ? GameInstance->GetSubsystem<UPRDiagnosticsSubsystem>() : nullptr;
	if (!Diagnostics)
	{
		StatusMessage = TEXT("Diagnostics export is unavailable.");
	}
	else
	{
		const FPRDiagnosticExportResult Result = Diagnostics->ExportSnapshot(Controller);
		StatusMessage = Result.bSuccess
			? FString::Printf(TEXT("Exported %d events to %s"), Result.EventCount, *Result.OutputPath)
			: Result.Diagnostic;
	}
	RefreshBody();
	return FReply::Handled();
}

FReply UPRDiagnosticsWidget::HandleClearEvents()
{
	APlayerController* Controller = LocalController.Get();
	UGameInstance* GameInstance = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameInstance() : nullptr;
	if (UPRDiagnosticsSubsystem* Diagnostics = GameInstance ? GameInstance->GetSubsystem<UPRDiagnosticsSubsystem>() : nullptr)
	{
		Diagnostics->ClearEvents();
		StatusMessage = TEXT("Structured diagnostic events cleared.");
	}
	RefreshBody();
	return FReply::Handled();
}

void UPRDiagnosticsWidget::RefreshBody()
{
	if (TitleText)
	{
		TitleText->SetText(FText::Format(
			FText::FromString(TEXT("ProjectRift Diagnostics  |  {0}")),
			TabLabel(ActiveTab)));
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(BuildBodyText()));
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(StatusMessage.IsEmpty() ? TEXT("F1 toggles this panel. Esc closes it.") : StatusMessage));
	}
	if (ToolsBox)
	{
		ToolsBox->SetVisibility(ActiveTab == EPRDiagnosticsTab::Tools ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

FString UPRDiagnosticsWidget::BuildBodyText() const
{
	APlayerController* Controller = LocalController.Get();
	UGameInstance* GameInstance = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameInstance() : nullptr;
	const UPRDiagnosticsSubsystem* Diagnostics = GameInstance ? GameInstance->GetSubsystem<UPRDiagnosticsSubsystem>() : nullptr;
	if (!Diagnostics)
	{
		return TEXT("Diagnostics subsystem unavailable.");
	}
	const FPRDiagnosticSnapshot Snapshot = Diagnostics->BuildSnapshot(Controller);

	switch (ActiveTab)
	{
	case EPRDiagnosticsTab::Overview:
		return FString::Printf(
			TEXT("Health: %s\nVersion: %s  |  Engine: %s  |  Build: %s\nMap: %s  |  World: %s  |  Net: %s  |  PIE: %d\nFPS: %.1f  |  Frame: %.2f ms  |  Uptime: %.1f s\n\n%s"),
			*StaticEnum<EPRDiagnosticHealth>()->GetNameStringByValue(static_cast<int64>(Snapshot.Health)),
			*Snapshot.Environment.ProjectVersion,
			*Snapshot.Environment.EngineVersion,
			*Snapshot.Environment.BuildConfiguration,
			*Snapshot.Environment.MapName,
			*Snapshot.Environment.WorldType,
			*Snapshot.Environment.NetMode,
			Snapshot.Environment.PIEInstance,
			Snapshot.Environment.AverageFps,
			Snapshot.Environment.AverageFrameTimeMs,
			Snapshot.Environment.UptimeSeconds,
			Snapshot.HealthMessages.IsEmpty() ? TEXT("No active health warnings.") : *FString::Join(Snapshot.HealthMessages, TEXT("\n")));
	case EPRDiagnosticsTab::Player:
		return FString::Printf(
			TEXT("Controller: %s\nPawn: %s\nPlayerState: %s\nActive profile: %s  (%s)\nBound profile: %s\nLast save: %s  |  Pending settlement: %s  |  Pending repair: %s\nRole: %s  |  Chapter: %s\nHealth %.1f / %.1f  |  Shield %.1f / %.1f  |  Energy %.1f / %.1f  |  Downed: %s\nInventory: %s\nResources: %s\nShip: %s"),
			*Snapshot.Player.ControllerName,
			*Snapshot.Player.PawnName,
			*Snapshot.Player.PlayerStateName,
			*Snapshot.Player.ActiveProfileDisplayName,
			*GuidOrNone(Snapshot.Player.ActiveProfileId),
			*GuidOrNone(Snapshot.Player.BoundProfileId),
			*Snapshot.Player.LastSaveStatus,
			Snapshot.Player.bPendingSettlement ? TEXT("YES") : TEXT("NO"),
			Snapshot.Player.bPendingRepair ? TEXT("YES") : TEXT("NO"),
			*Snapshot.Player.SelectedRoleId.ToString(),
			*Snapshot.Player.CurrentChapterId.ToString(),
			Snapshot.Player.Health, Snapshot.Player.MaxHealth,
			Snapshot.Player.Shield, Snapshot.Player.MaxShield,
			Snapshot.Player.Energy, Snapshot.Player.MaxEnergy,
			Snapshot.Player.bDowned ? TEXT("YES") : TEXT("NO"),
			*Snapshot.Player.InventorySummary,
			*Snapshot.Player.ResourceSummary,
			*Snapshot.Player.ShipModuleSummary);
	case EPRDiagnosticsTab::TeamAndRift:
	{
		FString Text = FString::Printf(
			TEXT("Session: %s\nTeam mission: %s  |  Ready: %s\nStart blocker: %s\n\nPlayers (%d):\n"),
			*Snapshot.Team.SessionState,
			*Snapshot.Team.SelectedMissionId.ToString(),
			Snapshot.Team.bTeamMissionReady ? TEXT("YES") : TEXT("NO"),
			Snapshot.Team.StartBlockReason.IsEmpty() ? TEXT("None") : *Snapshot.Team.StartBlockReason,
			Snapshot.Team.Players.Num());
		for (const FPRDiagnosticTeamPlayerSnapshot& Player : Snapshot.Team.Players)
		{
			Text += FString::Printf(
				TEXT("P%d %s | %s | Profile %s | Role %s | Repair %s\n"),
				Player.PlayerId,
				*Player.DisplayName,
				Player.bReady ? TEXT("READY") : TEXT("WAITING"),
				Player.bProfileBound ? TEXT("BOUND") : TEXT("UNBOUND"),
				*Player.SelectedRoleId.ToString(),
				Player.bRepairPending ? TEXT("PENDING") : TEXT("CLEAR"));
		}
		const FPREncounterDirectorSnapshot& Encounter = Snapshot.Rift.EncounterDirector;
		Text += FString::Printf(
			TEXT("\nRift available: %s  |  Mission: %s\nRun: %s\nSettlement: %s (%s)\nObjective: %s  %.1f%%  |  Stability: %.1f  |  Time: %.1f\nAlive: %d  |  Extracted: %d  |  Kills: %d\nDirector: %s %.1f/%.1f (%d AI), next %.1fs, last %s%s"),
			Snapshot.Rift.bAvailable ? TEXT("YES") : TEXT("NO"),
			*Snapshot.Rift.MissionId.ToString(),
			*GuidOrNone(Snapshot.Rift.RunId),
			*GuidOrNone(Snapshot.Rift.SettlementId),
			*Snapshot.Rift.SettlementStatus,
			*Snapshot.Rift.ObjectiveState,
			Snapshot.Rift.ObjectiveProgress * 100.0f,
			Snapshot.Rift.Stability,
			Snapshot.Rift.MissionTime,
			Snapshot.Rift.AlivePlayers,
			Snapshot.Rift.ExtractedPlayers,
			Snapshot.Rift.KilledEnemies,
			*StaticEnum<EPREncounterPhase>()->GetNameStringByValue(static_cast<int64>(Encounter.Phase)),
			Encounter.AliveThreat, Encounter.TargetThreatBudget, Encounter.AliveEnemyCount, Encounter.NextEventRemainingSeconds,
			*Encounter.LastDecisionCode.ToString(),
			Encounter.LastRejectionReason.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Encounter.LastRejectionReason));
		return Text;
	}
	case EPRDiagnosticsTab::Events:
	{
		FPRDiagnosticFilter Filter;
		Filter.MinimumSeverity = EPRDiagnosticSeverity::Verbose;
		const TArray<FPRDiagnosticEvent> Events = Diagnostics->GetEvents(Filter);
		if (Events.IsEmpty())
		{
			return TEXT("No structured diagnostic events recorded.");
		}
		FString Text;
		const int32 FirstIndex = FMath::Max(0, Events.Num() - 200);
		for (int32 Index = FirstIndex; Index < Events.Num(); ++Index)
		{
			const FPRDiagnosticEvent& Event = Events[Index];
			Text += FString::Printf(
				TEXT("#%lld  %s  [%s]  %s/%s\n%s\n\n"),
				Event.Sequence,
				*Event.TimestampUtc.ToString(TEXT("%H:%M:%S.%s")),
				*StaticEnum<EPRDiagnosticSeverity>()->GetNameStringByValue(static_cast<int64>(Event.Severity)),
				*Event.Category.ToString(),
				*Event.EventCode.ToString(),
				*Event.Message);
		}
		return Text;
	}
	case EPRDiagnosticsTab::Tools:
		return TEXT("Controlled ProjectA-local actions. Destructive fault actions remain disabled unless this instance uses an isolated Saved/Automation profile root.");
	default:
		return TEXT("Unknown diagnostics tab.");
	}
}

void UPRDiagnosticsWidget::RefreshTools()
{
	if (!ToolsBox)
	{
		return;
	}
	ToolsBox->ClearChildren();
	if (ActiveTab != EPRDiagnosticsTab::Tools)
	{
		return;
	}

	APlayerController* Controller = LocalController.Get();
	UGameInstance* GameInstance = Controller && Controller->GetWorld() ? Controller->GetWorld()->GetGameInstance() : nullptr;
	UPRDeveloperToolsSubsystem* DeveloperTools = GameInstance ? GameInstance->GetSubsystem<UPRDeveloperToolsSubsystem>() : nullptr;
	UPRSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	if (!DeveloperTools)
	{
		ToolsBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Developer tools unavailable.")))];
		return;
	}

	auto AddActionButton = [this, DeveloperTools, Controller](const FString& Label, const EPRDeveloperAction Action, const FGuid ProfileId = FGuid())
	{
		const FPRDeveloperActionAvailability Availability = DeveloperTools->GetActionAvailability(Action, Controller);
		ToolsBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.IsEnabled(Availability.bAvailable)
			.Text(FText::FromString(Label))
			.ToolTipText(FText::FromString(Availability.bAvailable ? TEXT("Available") : Availability.DisabledReason))
			.OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleDeveloperAction, Action, ProfileId)
		];
	};

	AddActionButton(TEXT("Create new developer profile"), EPRDeveloperAction::CreateProfile);
	AddActionButton(TEXT("Save safe lobby checkpoint"), EPRDeveloperAction::SaveCheckpoint);
	AddActionButton(TEXT("Create repeatable legacy v1 sample"), EPRDeveloperAction::CreateLegacyV1Profile);
	AddActionButton(TEXT("Prepare ship-repair acceptance profile"), EPRDeveloperAction::PrepareShipRepairAcceptance);
	AddActionButton(TEXT("Spawn test loot (rift only)"), EPRDeveloperAction::SpawnTestLoot);
	AddActionButton(TEXT("Toggle ready"), EPRDeveloperAction::ToggleReady);
	AddActionButton(TEXT("Start selected mission (host)"), EPRDeveloperAction::StartMission);
	AddActionButton(TEXT("CORRUPT active primary [two clicks, isolated root only]"), EPRDeveloperAction::CorruptActivePrimary);
	AddActionButton(TEXT("FAIL next save once [two clicks, isolated root only]"), EPRDeveloperAction::FailNextSave);

	ToolsBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)
	[
		SNew(SButton).Text(FText::FromString(TEXT("Export snapshot + structured events to JSON"))).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleExport)
	];
	ToolsBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
	[
		SNew(SButton).Text(FText::FromString(TEXT("Clear structured event buffer"))).OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleClearEvents)
	];

	if (SaveSubsystem)
	{
		ToolsBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
			.Text(FText::FromString(TEXT("Profiles")))
		];
		for (const FPRProfileSummary& Profile : SaveSubsystem->GetProfiles())
		{
			ToolsBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(
						TEXT("%s%s  |  v%d  |  %s"),
						Profile.bIsActive ? TEXT("[ACTIVE] ") : TEXT(""),
						*Profile.DisplayName,
						Profile.SaveVersion,
						*Profile.ProfileId.ToString(EGuidFormats::DigitsWithHyphensLower))))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled(!Profile.bIsActive)
					.Text(FText::FromString(TEXT("Continue")))
					.OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleDeveloperAction, EPRDeveloperAction::ActivateProfile, Profile.ProfileId)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Delete [two clicks]")))
					.OnClicked_UObject(this, &UPRDiagnosticsWidget::HandleDeveloperAction, EPRDeveloperAction::DeleteProfile, Profile.ProfileId)
				]
			];
		}
	}
}

bool UPRDiagnosticsWidget::IsMatchingConfirmation(const EPRDeveloperAction Action, const FGuid& ProfileId) const
{
	return bConfirmationPending && PendingConfirmationAction == Action && PendingConfirmationProfileId == ProfileId;
}

void UPRDiagnosticsWidget::ClearConfirmation()
{
	bConfirmationPending = false;
	PendingConfirmationProfileId.Invalidate();
}
