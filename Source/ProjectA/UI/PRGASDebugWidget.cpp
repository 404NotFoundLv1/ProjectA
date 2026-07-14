#include "UI/PRGASDebugWidget.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Characters/PRCharacter.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UPRGASDebugWidget::RebuildWidget()
{
	return SNew(SBorder)
		.Padding(FMargin(12.0f))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
		[
			SAssignNew(DebugTextBlock, STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
			.Text(FText::FromString(TEXT("ProjectRift GAS Debug\nWaiting for player state...")))
		];
}

void UPRGASDebugWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DebugTextBlock.IsValid())
	{
		DebugTextBlock->SetText(FText::FromString(GetDebugText()));
	}
}

FString UPRGASDebugWidget::GetDebugText() const
{
	const APlayerController* OwningController = GetOwningPlayer();
	const APRCharacter* ProjectRiftCharacter = OwningController ? Cast<APRCharacter>(OwningController->GetPawn()) : nullptr;
	const APRPlayerState* ProjectRiftPlayerState = nullptr;
	const UPRAttributeSet* AttributeSet = nullptr;
	const UPRAbilitySystemComponent* AbilitySystemComponent = nullptr;

	if (ProjectRiftCharacter)
	{
		ProjectRiftPlayerState = ProjectRiftCharacter->GetPlayerState<APRPlayerState>();
		AttributeSet = ProjectRiftCharacter->GetAttributeSet();
		AbilitySystemComponent = ProjectRiftCharacter->GetProjectRiftAbilitySystemComponent();
	}

	if (!ProjectRiftPlayerState && OwningController)
	{
		ProjectRiftPlayerState = OwningController->GetPlayerState<APRPlayerState>();
	}

	if (!AttributeSet && ProjectRiftPlayerState)
	{
		AttributeSet = ProjectRiftPlayerState->GetAttributeSet();
	}
	if (!AbilitySystemComponent && ProjectRiftPlayerState)
	{
		AbilitySystemComponent = ProjectRiftPlayerState->GetProjectRiftAbilitySystemComponent();
	}

	const FName SelectedRoleModule = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetSelectedRoleModule() : NAME_None;
	const FString RoleText = SelectedRoleModule.IsNone() ? TEXT("None") : SelectedRoleModule.ToString();

	if (!AttributeSet)
	{
		return FString::Printf(
			TEXT("ProjectRift GAS Debug\nPawn: %s\nAttributeSet: Missing\nDowned: %s\nRole: %s"),
			*GetNameSafe(ProjectRiftCharacter),
			ProjectRiftCharacter && ProjectRiftCharacter->IsDowned() ? TEXT("true") : TEXT("false"),
			*RoleText);
	}

	return FString::Printf(
		TEXT("ProjectRift GAS Debug\nHealth: %.0f / %.0f\nShield: %.0f / %.0f\nEnergy: %.0f / %.0f\nAttackPower: %.0f\nMoveSpeed: %.0f\nPollutionResistance: %.0f%%\nStatuses: %s\nDowned: %s\nRole: %s\nASC Ready: %s\nDefault GE: %s\nRole Abilities: %s"),
		AttributeSet->GetHealth(),
		AttributeSet->GetMaxHealth(),
		AttributeSet->GetShield(),
		AttributeSet->GetMaxShield(),
		AttributeSet->GetEnergy(),
		AttributeSet->GetMaxEnergy(),
		AttributeSet->GetAttackPower(),
		AttributeSet->GetMoveSpeed(),
		AttributeSet->GetPollutionResistance() * 100.0f,
		*UPRCombatEffectLibrary::GetActiveNegativeStatusText(AbilitySystemComponent),
		ProjectRiftCharacter && ProjectRiftCharacter->IsDowned() ? TEXT("true") : TEXT("false"),
		*RoleText,
		ProjectRiftCharacter && ProjectRiftCharacter->IsAbilitySystemInitialized() ? TEXT("true") : TEXT("false"),
		ProjectRiftCharacter && ProjectRiftCharacter->AreDefaultAttributesApplied() ? TEXT("true") : TEXT("false"),
		ProjectRiftCharacter && ProjectRiftCharacter->AreRoleModuleAbilitiesGranted() ? TEXT("true") : TEXT("false"));
}
