#include "UI/PRRoleLoadoutWidget.h"

#include "Core/PRAssetManager.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleComponent.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ProjectRiftRoleLoadoutWidgetPrivate
{
FSlateFontInfo GetFont(const float Size)
{
	static const TSharedPtr<const FCompositeFont> RoleFont = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"), FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
		EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	return FSlateFontInfo(RoleFont, Size);
}

const TCHAR* SlotName(const EPRRoleModuleSlot Slot)
{
	switch (Slot)
	{
	case EPRRoleModuleSlot::Q: return TEXT("Q");
	case EPRRoleModuleSlot::E: return TEXT("E");
	case EPRRoleModuleSlot::R: return TEXT("R");
	default: return TEXT("?");
	}
}
}

UPRRoleLoadoutWidget::UPRRoleLoadoutWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UPRRoleLoadoutWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(STextBlock).Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(24.0f))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.54f, 0.96f, 0.9f, 1.0f)))
				.Text(FText::FromString(TEXT("Role Loadout")))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).OnClicked_UObject(this, &UPRRoleLoadoutWidget::HandleCloseClicked)
				[
					SNew(STextBlock).Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(14.0f)).Text(FText::FromString(TEXT("Close [Esc]")))
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SBorder).Padding(14.0f).BorderBackgroundColor(FLinearColor(0.035f, 0.05f, 0.065f, 0.96f))
			[
				SAssignNew(SummaryTextBlock, STextBlock).AutoWrapText(true)
				.Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(16.0f))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.93f, 0.96f, 1.0f)))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 8.0f)
		[
			SAssignNew(StatusTextBlock, STextBlock).AutoWrapText(true)
			.Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(14.0f))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.35f, 1.0f)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton).OnClicked_UObject(this, &UPRRoleLoadoutWidget::HandleDefaultsClicked)
				[
					SNew(STextBlock).Justification(ETextJustify::Center).Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(16.0f)).Text(FText::FromString(TEXT("Restore Defaults")))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SButton).OnClicked_UObject(this, &UPRRoleLoadoutWidget::HandleApplyClicked)
				[
					SNew(STextBlock).Justification(ETextJustify::Center).Font(ProjectRiftRoleLoadoutWidgetPrivate::GetFont(16.0f)).Text(FText::FromString(TEXT("Apply")))
				]
			]
		];

	RefreshLoadoutDisplay();
	return SNew(SBorder).Padding(18.0f).BorderBackgroundColor(FLinearColor(0.008f, 0.014f, 0.022f, 0.98f))
		[
			SNew(SBox).MinDesiredWidth(620.0f).MinDesiredHeight(420.0f)[Content]
		];
}

FReply UPRRoleLoadoutWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPRRoleLoadoutWidget::RefreshLoadoutDisplay()
{
	if (SummaryTextBlock)
	{
		SummaryTextBlock->SetText(BuildLoadoutSummary());
	}
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(FText::FromString(LocalStatus.IsEmpty() ? TEXT("Configure only while waiting in the ship lobby.") : LocalStatus));
	}
}

void UPRRoleLoadoutWidget::RequestApplyLoadout()
{
	APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	UPRRoleComponent* RoleComponent = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	if (!Controller || !PlayerState || !RoleComponent)
	{
		HandleApplyResult(EPRRoleLoadoutApplyResult::InternalFailure, TEXT("Role state is unavailable."));
		return;
	}
	Controller->ServerApplyRoleLoadout(PlayerState->GetSelectedRoleModule(), RoleComponent->GetCurrentLoadout());
}

void UPRRoleLoadoutWidget::RequestRestoreDefaults()
{
	FName RoleId;
	FPRRoleLoadout DefaultLoadout;
	if (!ResolveCanonicalDefaults(RoleId, DefaultLoadout))
	{
		HandleApplyResult(EPRRoleLoadoutApplyResult::InvalidLoadout, TEXT("No valid role defaults are available."));
		return;
	}
	if (APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		LocalStatus = TEXT("Default loadout request sent to server.");
		Controller->ServerApplyRoleLoadout(RoleId, DefaultLoadout);
		RefreshLoadoutDisplay();
	}
}

void UPRRoleLoadoutWidget::RequestClose()
{
	if (APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		Controller->HideRoleLoadoutPanel();
	}
}

void UPRRoleLoadoutWidget::HandleApplyResult(const EPRRoleLoadoutApplyResult Result, const FString& Diagnostic)
{
	LocalStatus = Diagnostic.IsEmpty() ? FString::Printf(TEXT("Loadout result: %d"), static_cast<int32>(Result)) : Diagnostic;
	RefreshLoadoutDisplay();
}

FReply UPRRoleLoadoutWidget::HandleApplyClicked() { RequestApplyLoadout(); return FReply::Handled(); }
FReply UPRRoleLoadoutWidget::HandleDefaultsClicked() { RequestRestoreDefaults(); return FReply::Handled(); }
FReply UPRRoleLoadoutWidget::HandleCloseClicked() { RequestClose(); return FReply::Handled(); }

FText UPRRoleLoadoutWidget::BuildLoadoutSummary() const
{
	const APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	const APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	const UPRRoleComponent* Component = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	if (!PlayerState || !Component)
	{
		return FText::FromString(TEXT("Waiting for replicated role state..."));
	}
	FString Result = FString::Printf(TEXT("Role: %s\nReady: %s\n\n"), *PlayerState->GetSelectedRoleModule().ToString(), PlayerState->IsReady() ? TEXT("Yes") : TEXT("No"));
	for (const FPRRoleModuleSlotEntry& Entry : Component->GetCurrentLoadout().Entries)
	{
		Result += FString::Printf(TEXT("%s: %s\n"), ProjectRiftRoleLoadoutWidgetPrivate::SlotName(Entry.Slot), *Entry.ModuleId.ToString());
	}
	Result += FString::Printf(TEXT("\nUnlocked roles: %d | modules: %d"), Component->GetUnlockedRoleIds().Num(), Component->GetUnlockedModuleIds().Num());
	return FText::FromString(Result);
}

bool UPRRoleLoadoutWidget::ResolveCanonicalDefaults(FName& OutRoleId, FPRRoleLoadout& OutLoadout) const
{
	OutRoleId = NAME_None;
	OutLoadout.Entries.Reset();
	const APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	const APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	const UPRRoleComponent* Component = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	if (!Component || !AssetManager)
	{
		return false;
	}
	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> Modules;
	if (!AssetManager->LoadRoleCatalog(Roles, Modules))
	{
		return false;
	}
	const FName CurrentRoleId = PlayerState->GetSelectedRoleModule();
	UPRRoleDataAsset* const* Role = Roles.FindByPredicate([CurrentRoleId](const UPRRoleDataAsset* Candidate) { return Candidate && Candidate->RoleId == CurrentRoleId; });
	if (!Role || !*Role || !Component->GetUnlockedRoleIds().Contains(CurrentRoleId))
	{
		return false;
	}
	OutRoleId = CurrentRoleId;
	OutLoadout = (*Role)->DefaultLoadout;
	return Component->IsLoadoutValid(OutRoleId, OutLoadout);
}
