#include "UI/PRProfileDebugWidget.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerState.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PRProfileDebugWidget"

TSharedRef<SWidget> UPRProfileDebugWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox);
	RootBox = Panel;
	RefreshProfiles();
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f))
		[
			SNew(SBox)
			.MinDesiredWidth(480.0f)
			[
				Panel
			]
		];
}

void UPRProfileDebugWidget::RefreshProfiles()
{
	if (!RootBox.IsValid())
	{
		return;
	}
	RootBox->ClearChildren();
	RootBox->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Title", "ProjectRift v0.6.0 Legacy Profile Verification"))
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
		.ColorAndOpacity(FSlateColor(FLinearColor::White))
	];
	RootBox->AddSlot().AutoHeight().Padding(0, 2, 0, 4)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.0f, 7.0f))
			.OnClicked_UObject(this, &UPRProfileDebugWidget::HandleCreateProfile)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.Text(LOCTEXT("Create", "New Profile"))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.0f, 7.0f))
			.OnClicked_UObject(this, &UPRProfileDebugWidget::HandleSaveCheckpoint)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.Text(LOCTEXT("Save", "Save Checkpoint"))
			]
		]
	];
	RootBox->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.0f, 7.0f))
			.OnClicked_UObject(this, &UPRProfileDebugWidget::HandleCreateLegacyProfile)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.Text(LOCTEXT("Legacy", "Create v1 Sample"))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
		[
			SNew(SButton)
			.ContentPadding(FMargin(12.0f, 7.0f))
			.OnClicked_UObject(this, &UPRProfileDebugWidget::HandleCorruptPrimary)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.Text(LOCTEXT("Corrupt", "Corrupt Primary"))
			]
		]
	];

	UPRSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	const TArray<FPRProfileSummary> Profiles = SaveSubsystem ? SaveSubsystem->GetProfiles() : TArray<FPRProfileSummary>();
	TSharedRef<SVerticalBox> ProfileList = SNew(SVerticalBox);
	for (const FPRProfileSummary& Profile : Profiles)
	{
		ProfileList->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.Text(FText::FromString(FString::Printf(
					TEXT("%s%s  v%d  %s"),
					Profile.bIsActive ? TEXT("[Active] ") : TEXT(""),
					*Profile.DisplayName,
					Profile.SaveVersion,
					*Profile.ProfileId.ToString(EGuidFormats::DigitsWithHyphens))))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton).Text(LOCTEXT("Continue", "Continue")).OnClicked_UObject(this, &UPRProfileDebugWidget::HandleContinueProfile, Profile.ProfileId)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SButton).Text(LOCTEXT("Delete", "Delete")).OnClicked_UObject(this, &UPRProfileDebugWidget::HandleDeleteProfile, Profile.ProfileId)
			]
		];
	}
	if (Profiles.IsEmpty())
	{
		ProfileList->AddSlot().AutoHeight().Padding(2, 8)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
			.Text(LOCTEXT("NoProfiles", "No local profiles yet."))
		];
	}
	RootBox->AddSlot().FillHeight(1.0f).Padding(2, 0, 2, 4)
	[
		SNew(SBox)
		.MaxDesiredHeight(260.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				ProfileList
			]
		]
	];
	RootBox->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
	[
		SAssignNew(StatusText, STextBlock)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
		.Text(LOCTEXT("Ready", "Ready."))
	];
}

void UPRProfileDebugWidget::SetStatus(const FPRProfileOperationResult& Result)
{
	if (!StatusText.IsValid())
	{
		return;
	}
	FString Message = Result.IsSuccess() ? TEXT("Success") : FString::Printf(TEXT("Failed (%d)"), static_cast<int32>(Result.Status));
	if (Result.bMigrated) { Message += FString::Printf(TEXT("; migrated to v%d"), UPRProfileSave::LatestSaveVersion); }
	if (Result.bRecoveredFromBackup) { Message += TEXT("; primary corrupt, restored validated backup"); }
	if (!Result.Diagnostic.IsEmpty()) { Message += TEXT(": ") + Result.Diagnostic; }
	StatusText->SetText(FText::FromString(Message));
}

FReply UPRProfileDebugWidget::HandleCreateProfile()
{
	if (UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>())
	{
		const FPRProfileOperationResult Result = SaveSubsystem->CreateProfile(FString());
		RefreshProfiles();
		SetStatus(Result);
	}
	return FReply::Handled();
}

FReply UPRProfileDebugWidget::HandleContinueProfile(const FGuid ProfileId)
{
	if (UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>())
	{
		const FPRProfileOperationResult Result = SaveSubsystem->ActivateAndLoadProfile(ProfileId);
		RefreshProfiles();
		SetStatus(Result);
	}
	return FReply::Handled();
}

FReply UPRProfileDebugWidget::HandleDeleteProfile(const FGuid ProfileId)
{
	if (UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>())
	{
		const FPRProfileOperationResult Result = SaveSubsystem->DeleteProfile(ProfileId);
		RefreshProfiles();
		SetStatus(Result);
	}
	return FReply::Handled();
}

FReply UPRProfileDebugWidget::HandleSaveCheckpoint()
{
	FPRProfileOperationResult Result = FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::ValidationFailed, TEXT("No local PlayerState is available."));
	UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>();
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (SaveSubsystem && Controller)
	{
		Result = SaveSubsystem->CaptureAndSaveAtSafeCheckpoint(Controller->GetPlayerState<APRPlayerState>());
	}
	SetStatus(Result);
	return FReply::Handled();
}

FReply UPRProfileDebugWidget::HandleCreateLegacyProfile()
{
	if (UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>())
	{
		const FPRProfileOperationResult Result = SaveSubsystem->CreateLegacyV1ProfileForDevelopment();
		RefreshProfiles();
		SetStatus(Result);
	}
	return FReply::Handled();
}

FReply UPRProfileDebugWidget::HandleCorruptPrimary()
{
	if (UPRSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UPRSaveSubsystem>())
	{
		const FPRProfileOperationResult Result = SaveSubsystem->CorruptActivePrimaryForDevelopment();
		RefreshProfiles();
		SetStatus(Result);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
