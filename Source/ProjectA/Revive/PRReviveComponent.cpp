#include "Revive/PRReviveComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRRiftGameMode.h"
#include "Deployables/PRDeployableComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "Settings/PRProjectSettings.h"
#include "Weapons/PRWeaponComponent.h"

namespace
{
float ClampDuration(const float Value, const float Fallback)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.1f, 600.0f) : Fallback;
}

float ClampDistance(const float Value, const float Fallback)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 1.0f, 5000.0f) : Fallback;
}

float ClampFraction(const float Value, const float Fallback)
{
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.01f, 1.0f) : Fallback;
}
}

UPRReviveComponent::UPRReviveComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPRReviveComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UPRReviveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelRevive();
	Super::EndPlay(EndPlayReason);
}

void UPRReviveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPRReviveComponent, BleedOutEndServerTime);
	DOREPLIFETIME(UPRReviveComponent, ReviveStartServerTime);
	DOREPLIFETIME(UPRReviveComponent, ReviveDuration);
	DOREPLIFETIME(UPRReviveComponent, CurrentReviver);
	DOREPLIFETIME(UPRReviveComponent, ReviveSource);
}

APRCharacter* UPRReviveComponent::GetOwnerCharacter() const
{
	return Cast<APRCharacter>(GetOwner());
}

float UPRReviveComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

bool UPRReviveComponent::IsOwnerDownedAndNotDead() const
{
	const APRCharacter* Character = GetOwnerCharacter();
	const UPRAbilitySystemComponent* ASC = Character ? Character->GetProjectRiftAbilitySystemComponent() : nullptr;
	return Character && Character->IsDowned() && ASC
		&& !ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead);
}

bool UPRReviveComponent::EnterDownedState()
{
	APRCharacter* Character = GetOwnerCharacter();
	UPRAbilitySystemComponent* ASC = Character ? Character->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!Character || !Character->HasAuthority() || !ASC
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead))
	{
		return false;
	}
	if (Character->IsDowned())
	{
		return true;
	}

	ASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagOnly);
	ASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_BeingRevived, 1, EGameplayTagReplicationState::TagOnly);
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(ASC);
	ASC->CancelAllAbilities();
	if (APRPlayerState* PlayerState = Character->GetPlayerState<APRPlayerState>())
	{
		if (UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent())
		{
			Weapon->SetAiming(false);
			Weapon->CancelReload();
		}
		if (UPRDeployableComponent* Deployables = PlayerState->GetDeployableComponent())
		{
			Deployables->CancelPlacement(TEXT("Downed state cancelled deployment."));
		}
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	BleedOutEndServerTime = GetServerTimeSeconds() + ClampDuration(Settings ? Settings->BleedOutDuration : 30.0f, 30.0f);
	ClearReviveState();
	SetComponentTickEnabled(true);
	Character->ForceNetUpdate();
	if (APRRiftGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		GameMode->HandlePlayerDowned(Character);
	}
	return true;
}

bool UPRReviveComponent::HasReviveLineOfSight(const APRCharacter* Rescuer) const
{
	const APRCharacter* Target = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Target || !Rescuer || !World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PRReviveLineOfSight), false);
	Params.AddIgnoredActor(Target);
	Params.AddIgnoredActor(Rescuer);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		Rescuer->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f),
		Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f),
		ECC_Visibility,
		Params);
}

bool UPRReviveComponent::ValidatePlayerRevive(APRCharacter* Rescuer) const
{
	const APRCharacter* Target = GetOwnerCharacter();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float MaxDistance = ClampDistance(Settings ? Settings->ReviveInteractionDistance : 250.0f, 250.0f);
	const UPRAbilitySystemComponent* RescuerASC = Rescuer ? Rescuer->GetProjectRiftAbilitySystemComponent() : nullptr;
	const bool bReviverOccupiedByAnotherTarget = RescuerASC
		&& RescuerASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Reviving)
		&& CurrentReviver != Rescuer;
	return Target && Rescuer && Target != Rescuer && Target->GetWorld() == Rescuer->GetWorld()
		&& Target->HasAuthority() && IsOwnerDownedAndNotDead() && Rescuer->IsAlive()
		&& !Rescuer->IsDowned()
		&& !bReviverOccupiedByAnotherTarget
		&& FVector::DistSquared(Target->GetActorLocation(), Rescuer->GetActorLocation()) <= FMath::Square(MaxDistance)
		&& HasReviveLineOfSight(Rescuer);
}

bool UPRReviveComponent::CanBeRevivedBy(const APRCharacter* Rescuer) const
{
	const APRCharacter* Target = GetOwnerCharacter();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float MaxDistance = ClampDistance(Settings ? Settings->ReviveInteractionDistance : 250.0f, 250.0f);
	const UPRAbilitySystemComponent* TargetASC = Target ? Target->GetProjectRiftAbilitySystemComponent() : nullptr;
	const UPRAbilitySystemComponent* RescuerASC = Rescuer ? Rescuer->GetProjectRiftAbilitySystemComponent() : nullptr;
	return Target && Rescuer && Target != Rescuer && Target->GetWorld() == Rescuer->GetWorld()
		&& TargetASC && Target->IsDowned() && !TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
		&& RescuerASC && Rescuer->IsAlive() && !Rescuer->IsDowned()
		&& !RescuerASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Reviving)
		&& !IsReviveInProgress()
		&& FVector::DistSquared(Target->GetActorLocation(), Rescuer->GetActorLocation()) <= FMath::Square(MaxDistance)
		&& HasReviveLineOfSight(Rescuer);
}

bool UPRReviveComponent::BeginRevive(APRCharacter* Rescuer, const EPRReviveSource InSource, const float Duration)
{
	APRCharacter* Target = GetOwnerCharacter();
	UPRAbilitySystemComponent* TargetASC = Target ? Target->GetProjectRiftAbilitySystemComponent() : nullptr;
	UPRAbilitySystemComponent* RescuerASC = Rescuer ? Rescuer->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!Target || !TargetASC || !IsOwnerDownedAndNotDead() || IsReviveInProgress())
	{
		return false;
	}
	if (InSource == EPRReviveSource::Player && (!Rescuer || !RescuerASC))
	{
		return false;
	}

	CurrentReviver = Rescuer;
	ReviveSource = InSource;
	ReviveStartServerTime = GetServerTimeSeconds();
	ReviveDuration = ClampDuration(Duration, 3.0f);
	ReviverStartLocation = Rescuer ? Rescuer->GetActorLocation() : FVector::ZeroVector;
	TargetASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_BeingRevived, 1, EGameplayTagReplicationState::TagOnly);
	if (RescuerASC)
	{
		RescuerASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Reviving, 1, EGameplayTagReplicationState::TagOnly);
		Rescuer->OnHealthChanged.AddDynamic(this, &UPRReviveComponent::HandleReviverHealthChanged);
		Rescuer->OnShieldChanged.AddDynamic(this, &UPRReviveComponent::HandleReviverShieldChanged);
		if (APRPlayerState* PlayerState = Rescuer->GetPlayerState<APRPlayerState>())
		{
			if (UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent())
			{
				Weapon->SetAiming(false);
				Weapon->CancelReload();
			}
			if (UPRDeployableComponent* Deployables = PlayerState->GetDeployableComponent())
			{
				Deployables->CancelPlacement(TEXT("Revive interaction cancelled deployment."));
			}
		}
	}
	Target->ForceNetUpdate();
	return true;
}

bool UPRReviveComponent::BeginPlayerRevive(APRCharacter* Rescuer)
{
	if (!ValidatePlayerRevive(Rescuer))
	{
		return false;
	}
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	return BeginRevive(Rescuer, EPRReviveSource::Player, Settings ? Settings->ReviveHoldDuration : 3.0f);
}

bool UPRReviveComponent::BeginDroneRevive()
{
	if (!IsOwnerDownedAndNotDead() || IsReviveInProgress())
	{
		return false;
	}
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	return BeginRevive(nullptr, EPRReviveSource::RescueDrone, Settings ? Settings->DroneReviveDuration : 3.0f);
}

void UPRReviveComponent::ClearReviveState()
{
	APRCharacter* Target = GetOwnerCharacter();
	if (CurrentReviver)
	{
		CurrentReviver->OnHealthChanged.RemoveDynamic(this, &UPRReviveComponent::HandleReviverHealthChanged);
		CurrentReviver->OnShieldChanged.RemoveDynamic(this, &UPRReviveComponent::HandleReviverShieldChanged);
		if (UPRAbilitySystemComponent* RescuerASC = CurrentReviver->GetProjectRiftAbilitySystemComponent())
		{
			RescuerASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_Reviving, 1, EGameplayTagReplicationState::TagOnly);
		}
	}
	if (UPRAbilitySystemComponent* TargetASC = Target ? Target->GetProjectRiftAbilitySystemComponent() : nullptr)
	{
		TargetASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_BeingRevived, 1, EGameplayTagReplicationState::TagOnly);
	}
	CurrentReviver = nullptr;
	ReviveSource = EPRReviveSource::None;
	ReviveStartServerTime = 0.0f;
	ReviveDuration = 0.0f;
	ReviverStartLocation = FVector::ZeroVector;
}

void UPRReviveComponent::CancelRevive(APRCharacter* RequestingRescuer)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsReviveInProgress())
	{
		return;
	}
	if (RequestingRescuer && ReviveSource == EPRReviveSource::Player && CurrentReviver != RequestingRescuer)
	{
		return;
	}
	ClearReviveState();
	GetOwner()->ForceNetUpdate();
}

bool UPRReviveComponent::CompleteRevive(const EPRReviveSource RequestedSource)
{
	APRCharacter* Target = GetOwnerCharacter();
	UPRAbilitySystemComponent* ASC = Target ? Target->GetProjectRiftAbilitySystemComponent() : nullptr;
	UPRAttributeSet* Attributes = Target ? Target->GetAttributeSet() : nullptr;
	if (!Target || !Target->HasAuthority() || !ASC || !Attributes || !IsOwnerDownedAndNotDead()
		|| (RequestedSource != EPRReviveSource::System && ReviveSource != RequestedSource))
	{
		return false;
	}
	ClearReviveState();
	ASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagOnly);
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	Attributes->SetHealth(Attributes->GetMaxHealth() * ClampFraction(Settings ? Settings->ReviveHealthFraction : 0.4f, 0.4f));
	Attributes->SetShield(0.0f);
	BleedOutEndServerTime = 0.0f;
	if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	Target->RefreshMovementFromAttributes();
	SetComponentTickEnabled(false);
	Target->ForceNetUpdate();
	if (APRRiftGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		GameMode->HandlePlayerRevived(Target);
	}
	return true;
}

void UPRReviveComponent::CompleteBleedOut()
{
	APRCharacter* Target = GetOwnerCharacter();
	UPRAbilitySystemComponent* ASC = Target ? Target->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!Target || !ASC || !IsOwnerDownedAndNotDead())
	{
		return;
	}
	ClearReviveState();
	ASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_Downed, 1, EGameplayTagReplicationState::TagOnly);
	ASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
	ASC->CancelAllAbilities();
	BleedOutEndServerTime = 0.0f;
	SetComponentTickEnabled(false);
	Target->ForceNetUpdate();
	if (APRRiftGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		GameMode->HandlePlayerBleedOut(Target);
	}
}

void UPRReviveComponent::CancelForMissionEnd()
{
	CancelRevive();
	SetComponentTickEnabled(false);
}

void UPRReviveComponent::HandleReviverHealthChanged(const float OldValue, const float NewValue)
{
	if (NewValue + KINDA_SMALL_NUMBER < OldValue)
	{
		CancelRevive(CurrentReviver);
	}
}

void UPRReviveComponent::HandleReviverShieldChanged(const float OldValue, const float NewValue)
{
	if (NewValue + KINDA_SMALL_NUMBER < OldValue)
	{
		CancelRevive(CurrentReviver);
	}
}

float UPRReviveComponent::GetBleedOutRemainingSeconds() const
{
	return IsOwnerDownedAndNotDead() ? FMath::Max(0.0f, BleedOutEndServerTime - GetServerTimeSeconds()) : 0.0f;
}

float UPRReviveComponent::GetReviveProgress() const
{
	return ReviveSource != EPRReviveSource::None && ReviveDuration > 0.0f
		? FMath::Clamp((GetServerTimeSeconds() - ReviveStartServerTime) / ReviveDuration, 0.0f, 1.0f)
		: 0.0f;
}

void UPRReviveComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsOwnerDownedAndNotDead())
	{
		SetComponentTickEnabled(false);
		return;
	}
	const float Now = GetServerTimeSeconds();
	if (Now >= BleedOutEndServerTime)
	{
		CompleteBleedOut();
		return;
	}
	if (ReviveSource == EPRReviveSource::Player)
	{
		const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
		const float MovementThreshold = ClampDistance(Settings ? Settings->ReviveMovementCancelDistance : 35.0f, 35.0f);
		if (!ValidatePlayerRevive(CurrentReviver)
			|| FVector::DistSquared(CurrentReviver->GetActorLocation(), ReviverStartLocation) > FMath::Square(MovementThreshold))
		{
			CancelRevive();
			return;
		}
	}
	if (ReviveSource != EPRReviveSource::None && Now >= ReviveStartServerTime + ReviveDuration)
	{
		CompleteRevive(ReviveSource);
	}
}
