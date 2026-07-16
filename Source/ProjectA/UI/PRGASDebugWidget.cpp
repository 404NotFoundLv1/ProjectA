#include "UI/PRGASDebugWidget.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Core/PRGameplayTags.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Player/PRPlayerController.h"
#include "Roles/PRRoleComponent.h"
#include "Items/PRWeaponDataAsset.h"
#include "Weapons/PRWeaponComponent.h"
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
			.Text(FText::FromString(TEXT("ProjectRift v0.6.5 GAS Debug\nWaiting for player state...")))
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
	const UPRRoleComponent* RoleComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetRoleComponent() : nullptr;
	FString LoadoutText;
	if (RoleComponent)
	{
		for (const FPRRoleModuleSlotEntry& Entry : RoleComponent->GetCurrentLoadout().Entries)
		{
			LoadoutText += FString::Printf(TEXT("%s=%s "), *StaticEnum<EPRRoleModuleSlot>()->GetNameStringByValue(static_cast<int64>(Entry.Slot)), *Entry.ModuleId.ToString());
		}
	}
	const UPRWeaponComponent* Weapon = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetWeaponComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = Weapon ? Weapon->GetEquippedWeaponData() : nullptr;
	const UPRCombatFeedbackComponent* CombatFeedback = ProjectRiftCharacter
		? ProjectRiftCharacter->FindComponentByClass<UPRCombatFeedbackComponent>()
		: nullptr;
	const APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(OwningController);
	const FString LastCueText = CombatFeedback && CombatFeedback->LastHandledCueTag.IsValid()
		? CombatFeedback->LastHandledCueTag.ToString()
		: TEXT("None");
	const FString WeaponText = WeaponData
		? (WeaponData->DisplayName.IsEmpty() ? WeaponData->ItemId.ToString() : WeaponData->DisplayName.ToString())
		: TEXT("None");
	const FString CooldownText = GetCooldownDebugText(AbilitySystemComponent);

	if (!AttributeSet)
	{
		return FString::Printf(
			TEXT("ProjectRift v0.6.5 GAS Debug\nPawn: %s\nAttributeSet: Missing\nDowned: %s\nRole: %s\nLoadout: %s\nCooldowns: %s"),
			*GetNameSafe(ProjectRiftCharacter),
			ProjectRiftCharacter && ProjectRiftCharacter->IsDowned() ? TEXT("true") : TEXT("false"),
			*RoleText,
			*LoadoutText,
			*CooldownText);
	}

	return FString::Printf(
		TEXT("ProjectRift v0.6.5 GAS Debug\nHealth: %.0f / %.0f\nShield: %.0f / %.0f\nEnergy: %.0f / %.0f\nAttackPower: %.0f\nMoveSpeed: %.0f\nPollutionResistance: %.0f%%\nStatuses: %s\nHitStaggered: %s\nLast Cue: %s\nCue Active/Handled: %d / %d\nHit Confirm Sent/Received: %d / %d\nWeapon: %s\nAmmo: %d / %d\nAiming: %s\nReloading: %s\nDowned: %s\nRole: %s\nLoadout: %s\nCooldowns: %s\nASC Ready: %s\nDefault GE: %s\nRole Abilities: %s"),
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
		AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered) ? TEXT("true") : TEXT("false"),
		*LastCueText,
		CombatFeedback ? CombatFeedback->ActiveStatusCueTags.Num() : 0,
		CombatFeedback ? CombatFeedback->GameplayCueHandledCount : 0,
		ProjectRiftController ? ProjectRiftController->HitConfirmationSentCount : 0,
		ProjectRiftController ? ProjectRiftController->HitConfirmationReceivedCount : 0,
		*WeaponText,
		Weapon ? Weapon->GetMagazineAmmo() : 0,
		Weapon ? Weapon->GetReserveAmmo() : 0,
		Weapon && Weapon->IsAiming() ? TEXT("true") : TEXT("false"),
		Weapon && Weapon->IsReloading() ? TEXT("true") : TEXT("false"),
		ProjectRiftCharacter && ProjectRiftCharacter->IsDowned() ? TEXT("true") : TEXT("false"),
		*RoleText,
		*LoadoutText,
		*CooldownText,
		ProjectRiftCharacter && ProjectRiftCharacter->IsAbilitySystemInitialized() ? TEXT("true") : TEXT("false"),
		ProjectRiftCharacter && ProjectRiftCharacter->AreDefaultAttributesApplied() ? TEXT("true") : TEXT("false"),
		ProjectRiftCharacter && ProjectRiftCharacter->AreRoleModuleAbilitiesGranted() ? TEXT("true") : TEXT("false"));
}

FString UPRGASDebugWidget::GetCooldownDebugText(const UAbilitySystemComponent* AbilitySystemComponent)
{
	auto FormatCooldown = [AbilitySystemComponent](const TCHAR* SlotName, const FGameplayTag& CooldownTag)
	{
		// GetActiveEffectsTimeRemaining requires a fully initialized ability actor info.
		if (!AbilitySystemComponent || !AbilitySystemComponent->GetOwnerActor())
		{
			return FString::Printf(TEXT("%s: Unavailable"), SlotName);
		}

		FGameplayTagContainer CooldownTags;
		CooldownTags.AddTag(CooldownTag);
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(CooldownTags);
		const TArray<float> RemainingTimes = AbilitySystemComponent->GetActiveEffectsTimeRemaining(Query);
		if (RemainingTimes.IsEmpty())
		{
			return FString::Printf(TEXT("%s: Ready"), SlotName);
		}

		float MaxRemainingSeconds = 0.0f;
		for (const float RemainingSeconds : RemainingTimes)
		{
			MaxRemainingSeconds = FMath::Max(MaxRemainingSeconds, RemainingSeconds);
		}
		return FString::Printf(TEXT("%s: Cooldown %.1fs"), SlotName, static_cast<double>(MaxRemainingSeconds));
	};

	const FString QCooldownText = FormatCooldown(TEXT("Q"), ProjectRiftGameplayTags::Cooldown_Skill_Q);
	const FString ECooldownText = FormatCooldown(TEXT("E"), ProjectRiftGameplayTags::Cooldown_Skill_E);
	const FString RCooldownText = FormatCooldown(TEXT("R"), ProjectRiftGameplayTags::Cooldown_Skill_R);
	return FString::Printf(TEXT("%s | %s | %s"), *QCooldownText, *ECooldownText, *RCooldownText);
}
