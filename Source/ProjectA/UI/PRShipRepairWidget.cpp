#include "UI/PRShipRepairWidget.h"

#include "Core/PRAssetManager.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Ship/PRShipRepairDataAsset.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ProjectRiftShipRepairWidgetPrivate
{
FSlateFontInfo GetFont(const float Size)
{
	static const TSharedPtr<const FCompositeFont> RepairFont = MakeShared<FStandaloneCompositeFont>(
		TEXT("Default"),
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
		EFontHinting::Default,
		EFontLoadingPolicy::LazyLoad);
	return FSlateFontInfo(RepairFont, Size);
}

FString AvailabilityToString(const EPRShipRepairAvailability Availability)
{
	switch (Availability)
	{
	case EPRShipRepairAvailability::Available: return TEXT("可修复");
	case EPRShipRepairAvailability::AlreadyCompleted: return TEXT("已完成");
	case EPRShipRepairAvailability::MissingPrerequisiteRepair: return TEXT("缺少前置修复");
	case EPRShipRepairAvailability::MissingStoryProgress: return TEXT("剧情前置不足");
	case EPRShipRepairAvailability::InsufficientResources: return TEXT("资源不足");
	case EPRShipRepairAvailability::InvalidContract:
	default: return TEXT("合同不可用");
	}
}
}

UPRShipRepairWidget::UPRShipRepairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UPRShipRepairWidget::RebuildWidget()
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(24.0f))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.54f, 0.96f, 0.9f, 1.0f)))
				.Text(FText::FromString(TEXT("舰船修复终端")))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.OnClicked_UObject(this, &UPRShipRepairWidget::HandleCloseClicked)
				[
					SNew(STextBlock)
					.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(14.0f))
					.Text(FText::FromString(TEXT("关闭 [Esc]")))
				]
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SBorder)
			.Padding(14.0f)
			.BorderBackgroundColor(FLinearColor(0.035f, 0.05f, 0.065f, 0.96f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(SummaryTextBlock, STextBlock)
					.AutoWrapText(true)
					.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(16.0f))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.93f, 0.96f, 1.0f)))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 8.0f)
		[
			SAssignNew(StatusTextBlock, STextBlock)
			.AutoWrapText(true)
			.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(14.0f))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.35f, 1.0f)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.IsEnabled_Lambda([this]() { return CanRequestRepair(); })
				.OnClicked_UObject(this, &UPRShipRepairWidget::HandleRepairClicked)
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(16.0f))
					.Text(FText::FromString(TEXT("执行修复")))
				]
			]
#if !UE_BUILD_SHIPPING
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.OnClicked_UObject(this, &UPRShipRepairWidget::HandlePrepareClicked)
				.ToolTipText(FText::FromString(TEXT("仅补齐剧情前置并把 EnergyCrystal 提升到最低需求，不会直接完成修复。")))
				[
					SNew(STextBlock)
					.Font(ProjectRiftShipRepairWidgetPrivate::GetFont(14.0f))
					.Text(FText::FromString(TEXT("[开发验收] 准备档案")))
				]
			]
#endif
		];

	TSharedRef<SWidget> Widget = SNew(SBorder)
		.Padding(18.0f)
		.BorderBackgroundColor(FLinearColor(0.008f, 0.014f, 0.022f, 0.98f))
		[
			SNew(SBox)
			.MinDesiredWidth(680.0f)
			.MinDesiredHeight(460.0f)
			[
				Content
			]
		];
	RefreshRepairDisplay();
	return Widget;
}

FReply UPRShipRepairWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPRShipRepairWidget::RefreshRepairDisplay()
{
	if (SummaryTextBlock)
	{
		SummaryTextBlock->SetText(BuildRepairSummary());
	}
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(BuildRepairStatus());
	}
}

void UPRShipRepairWidget::RequestDisplayedRepair()
{
	if (APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		LocalStatus = TEXT("修复请求已发送，等待服务器裁决……");
		Controller->RequestShipRepair(DisplayedRepairProjectId);
		RefreshRepairDisplay();
	}
}

void UPRShipRepairWidget::RequestClose()
{
	if (APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		Controller->HideShipRepairPanel();
	}
}

bool UPRShipRepairWidget::IsAcceptancePreparationAvailable() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

void UPRShipRepairWidget::PrepareAcceptanceProfile()
{
#if !UE_BUILD_SHIPPING
	UPRSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		LocalStatus = TEXT("无法访问本地档案子系统。");
		RefreshRepairDisplay();
		return;
	}
	const FPRProfileOperationResult Result = SaveSubsystem->PrepareShipRepairAcceptanceForDevelopment();
	LocalStatus = Result.IsSuccess() ? TEXT("验收档案已准备并安全保存。") : Result.Diagnostic;
	if (Result.IsSuccess())
	{
		if (APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer()))
		{
			Controller->SubmitLocalMultiplayerProfile();
		}
	}
	RefreshRepairDisplay();
#endif
}

FReply UPRShipRepairWidget::HandleRepairClicked()
{
	RequestDisplayedRepair();
	return FReply::Handled();
}

FReply UPRShipRepairWidget::HandleCloseClicked()
{
	RequestClose();
	return FReply::Handled();
}

FReply UPRShipRepairWidget::HandlePrepareClicked()
{
	PrepareAcceptanceProfile();
	return FReply::Handled();
}

FText UPRShipRepairWidget::BuildRepairSummary() const
{
	UPRSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	FPRProfileSnapshot Snapshot;
	const bool bHasSnapshot = SaveSubsystem && SaveSubsystem->GetActiveProfileSnapshot(Snapshot);
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	const UPRShipRepairDataAsset* Contract = AssetManager ? AssetManager->LoadShipRepairSync(DisplayedRepairProjectId) : nullptr;
	if (!bHasSnapshot || !Contract)
	{
		return FText::FromString(TEXT("没有可用的活动档案或修复合同。"));
	}

	const FPRShipRepairEvaluation Evaluation = SaveSubsystem->EvaluateShipRepair(DisplayedRepairProjectId);
	const FPRProfileShipModuleState* Module = Snapshot.ShipModules.FindByPredicate([Contract](const FPRProfileShipModuleState& Entry)
	{
		return Entry.ModuleId == Contract->ModuleId;
	});
	TArray<FString> CostParts;
	for (const FPRShipRepairResourceCost& Cost : Contract->ResourceCosts)
	{
		CostParts.Add(FString::Printf(TEXT("%s ×%d（当前 %d）"), *Cost.ResourceId.ToString(), Cost.Count, Snapshot.GetResourceCount(Cost.ResourceId)));
	}
	TArray<FString> StoryParts;
	for (const FName StoryNodeId : Contract->RequiredCompletedStoryNodeIds)
	{
		StoryParts.Add(FString::Printf(TEXT("%s：%s"), *StoryNodeId.ToString(), Snapshot.Story.CompletedStoryNodeIds.Contains(StoryNodeId) ? TEXT("已完成") : TEXT("未完成")));
	}
	return FText::FromString(FString::Printf(
		TEXT("项目：%s\n%s\n\n当前章节：%s\n目标模块：%s\n当前等级：%d　目标等级：%d\n\n资源成本：%s\n剧情前置：%s\n\n状态：%s\n诊断：%s"),
		*Contract->DisplayName.ToString(),
		*Contract->Description.ToString(),
		Snapshot.Story.CurrentChapterId.IsNone() ? TEXT("无") : *Snapshot.Story.CurrentChapterId.ToString(),
		*Contract->ModuleId.ToString(),
		Module ? Module->Level : 0,
		Contract->TargetLevel,
		CostParts.IsEmpty() ? TEXT("无") : *FString::Join(CostParts, TEXT("，")),
		StoryParts.IsEmpty() ? TEXT("无") : *FString::Join(StoryParts, TEXT("；")),
		*ProjectRiftShipRepairWidgetPrivate::AvailabilityToString(Evaluation.Availability),
		*Evaluation.Diagnostic));
}

FText UPRShipRepairWidget::BuildRepairStatus() const
{
	const APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	const APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	if (PlayerState && PlayerState->IsRepairPersistencePending())
	{
		return FText::FromString(FString::Printf(TEXT("等待本地安全保存；每 5 秒自动重试。%s"), Controller ? *Controller->GetShipRepairSaveStatus() : TEXT("")));
	}
	if (Controller && Controller->GetShipRepairSaveStatus() != TEXT("Idle"))
	{
		return FText::FromString(Controller->GetShipRepairSaveStatus());
	}
	return FText::FromString(LocalStatus.IsEmpty() ? TEXT("就绪") : LocalStatus);
}

bool UPRShipRepairWidget::CanRequestRepair() const
{
	const APRPlayerController* Controller = Cast<APRPlayerController>(GetOwningPlayer());
	const APRPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APRPlayerState>() : nullptr;
	UPRSaveSubsystem* SaveSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	return PlayerState && PlayerState->IsMultiplayerProfileBound() && !PlayerState->IsReady()
		&& !PlayerState->IsRepairPersistencePending() && SaveSubsystem
		&& SaveSubsystem->EvaluateShipRepair(DisplayedRepairProjectId).IsAvailable();
}
