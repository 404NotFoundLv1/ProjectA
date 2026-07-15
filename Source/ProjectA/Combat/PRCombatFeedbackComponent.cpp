#include "Combat/PRCombatFeedbackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatHitCameraShake.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PRGameplayTags.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Persistence/PRProfileTypes.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "TimerManager.h"
#include "UI/PRWeaponHUDWidget.h"

namespace
{
constexpr int32 MaxNetworkSafeCueFeedbackSequence = 31;

bool IsPersistentStatusCue(const FGameplayTag CueTag)
{
	return CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted
		|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed
		|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned;
}

bool IsImpactCue(const FGameplayTag CueTag)
{
	return CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield
		|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak
		|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health
		|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal;
}

FLinearColor GetCueTint(const FGameplayTag CueTag)
{
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield)
	{
		return FLinearColor(0.05f, 0.75f, 1.0f, 1.0f);
	}
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak)
	{
		return FLinearColor(0.25f, 1.0f, 1.0f, 1.0f);
	}
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal)
	{
		return FLinearColor(1.0f, 0.02f, 0.02f, 1.0f);
	}
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health)
	{
		return FLinearColor(1.0f, 0.22f, 0.12f, 1.0f);
	}
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted)
	{
		return FLinearColor(0.18f, 0.8f, 0.12f, 1.0f);
	}
	if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed)
	{
		return FLinearColor(0.16f, 0.42f, 1.0f, 1.0f);
	}
	return FLinearColor(1.0f, 0.72f, 0.12f, 1.0f);
}

EPRHitReactionStrength GetCueReactionStrength(const FGameplayCueParameters& Parameters)
{
	if (!FMath::IsFinite(Parameters.NormalizedMagnitude) || Parameters.NormalizedMagnitude <= 0.0f)
	{
		return EPRHitReactionStrength::None;
	}
	return Parameters.NormalizedMagnitude >= 0.75f
		? EPRHitReactionStrength::Heavy
		: EPRHitReactionStrength::Light;
}

template <typename TObjectType>
TObjectType* LoadSoftObjectIfAvailable(const TSoftObjectPtr<TObjectType>& Asset)
{
	if (Asset.IsNull())
	{
		return nullptr;
	}
	if (TObjectType* LoadedObject = Asset.Get())
	{
		return LoadedObject;
	}

	const FSoftObjectPath ObjectPath = Asset.ToSoftObjectPath();
	return FPackageName::DoesPackageExist(ObjectPath.GetLongPackageName())
		? Asset.LoadSynchronous()
		: nullptr;
}
}

UPRCombatFeedbackComponent::UPRCombatFeedbackComponent()
	: FrontLightHitMontage(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Lgt_01.MM_HitReact_Front_Lgt_01")))
	, FrontHeavyHitMontage(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Hvy_01.MM_HitReact_Front_Hvy_01")))
	, BackHitMontage(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Back_Med_01.MM_HitReact_Back_Med_01")))
	, FeedbackOverlayMaterial(FSoftObjectPath(TEXT("/Game/ProjectRift/GameplayCues/Materials/M_CombatFeedbackOverlay.M_CombatFeedbackOverlay")))
	, ImpactParticleSystem(FSoftObjectPath(TEXT("/Game/ProjectRift/GameplayCues/FX/P_CombatImpactPlaceholder.P_CombatImpactPlaceholder")))
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UPRCombatFeedbackComponent::ResolveCameraShakeScale(const bool bReduceCameraShake)
{
	return bReduceCameraShake ? 0.35f : 1.0f;
}

void UPRCombatFeedbackComponent::RecordResolvedDamage(
	const FPRResolvedDamage& ResolvedDamage,
	const FGameplayEffectContextHandle& EffectContext)
{
	LastResolvedDamage = ResolvedDamage;
	LastDamageEffectContext = EffectContext;
	++FeedbackDispatchCount;
	LastDamageFeedbackSequence = LastDamageFeedbackSequence > 0
		&& LastDamageFeedbackSequence < MaxNetworkSafeCueFeedbackSequence
		? LastDamageFeedbackSequence + 1
		: 1;

	LastDamageCueTags.Reset();
	if (ResolvedDamage.ShieldDamage > 0.0f)
	{
		LastDamageCueTags.AddTag(ResolvedDamage.bShieldBroken
			? ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak
			: ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield);
	}
	if (ResolvedDamage.HealthDamage > 0.0f)
	{
		LastDamageCueTags.AddTag(ResolvedDamage.bLethal
			? ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal
			: ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health);
	}
	GameplayCueDispatchCount += LastDamageCueTags.Num();
	bResolvedDamagePulsePending = !LastDamageCueTags.IsEmpty();
}

void UPRCombatFeedbackComponent::HandleGameplayCue(
	const FGameplayTag CueTag,
	const EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (!CueTag.IsValid())
	{
		return;
	}

	LastHandledCueTag = CueTag;
	++GameplayCueHandledCount;
	if (!IsPersistentStatusCue(CueTag))
	{
		if (EventType == EGameplayCueEvent::Executed && IsImpactCue(CueTag))
		{
			PlayImpactPresentation(CueTag, Parameters);
		}
		return;
	}

	if (EventType == EGameplayCueEvent::OnActive || EventType == EGameplayCueEvent::WhileActive)
	{
		ActiveStatusCueTags.AddTag(CueTag);
		RefreshPersistentOverlay();
	}
	else if (EventType == EGameplayCueEvent::Removed)
	{
		ActiveStatusCueTags.RemoveTag(CueTag);
		RefreshPersistentOverlay();
	}
}

void UPRCombatFeedbackComponent::SetActiveStagger(
	const FActiveGameplayEffectHandle Handle,
	const EPRHitReactionStrength Strength)
{
	ActiveStaggerHandle = Handle;
	ActiveStaggerStrength = Strength;
}

void UPRCombatFeedbackComponent::ClearActiveStagger()
{
	ActiveStaggerHandle = FActiveGameplayEffectHandle();
	ActiveStaggerStrength = EPRHitReactionStrength::None;
}

void UPRCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OverlayTimerHandle);
	}
	RestoreOriginalOverlay();
	Super::EndPlay(EndPlayReason);
}

USkeletalMeshComponent* UPRCombatFeedbackComponent::ResolveSkeletalMesh() const
{
	const AActor* OwnerActor = GetOwner();
	if (const ACharacter* Character = Cast<ACharacter>(OwnerActor))
	{
		return Character->GetMesh();
	}
	return OwnerActor ? OwnerActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UPRCombatFeedbackComponent::PlayImpactPresentation(
	const FGameplayTag CueTag,
	const FGameplayCueParameters& Parameters)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	bool bShouldEmitLocalHitPulse = false;
	if (bResolvedDamagePulsePending)
	{
		bResolvedDamagePulsePending = false;
		LastPulsedFeedbackSequence = LastDamageFeedbackSequence;
		bShouldEmitLocalHitPulse = true;
	}
	else if (Parameters.AbilityLevel > 0
		&& Parameters.AbilityLevel != LastPulsedFeedbackSequence
		&& FMath::IsFinite(Parameters.RawMagnitude)
		&& Parameters.RawMagnitude > 0.0f)
	{
		LastPulsedFeedbackSequence = Parameters.AbilityLevel;
		bShouldEmitLocalHitPulse = true;
	}
	if (bShouldEmitLocalHitPulse)
	{
		++LocalHitPulseCount;
	}

	const FVector RequestedImpactLocation(Parameters.Location);
	const FVector ImpactLocation = !RequestedImpactLocation.ContainsNaN()
		&& !RequestedImpactLocation.IsNearlyZero()
		? RequestedImpactLocation
		: OwnerActor->GetActorLocation();
	const FVector RequestedImpactNormal(Parameters.Normal);
	const FVector ImpactNormal = !RequestedImpactNormal.ContainsNaN()
		&& !RequestedImpactNormal.IsNearlyZero()
		? RequestedImpactNormal.GetSafeNormal()
		: FVector::UpVector;
	const AActor* OriginalInstigator = Parameters.EffectContext.GetOriginalInstigator();
	const AActor* PresentationInstigator = OriginalInstigator ? OriginalInstigator : Parameters.Instigator.Get();
	FVector IncomingDirection = PresentationInstigator
		? (PresentationInstigator->GetActorLocation() - OwnerActor->GetActorLocation()).GetSafeNormal()
		: ImpactNormal.GetSafeNormal();
	if (IncomingDirection.IsNearlyZero() || IncomingDirection.ContainsNaN())
	{
		IncomingDirection = -OwnerActor->GetActorForwardVector().GetSafeNormal();
	}
	const FVector IncomingDirection2D = IncomingDirection.GetSafeNormal2D();
	bLastImpactFromBack = !IncomingDirection2D.IsNearlyZero()
		&& FVector::DotProduct(OwnerActor->GetActorForwardVector(), IncomingDirection2D) < 0.0f;

	if (UParticleSystem* ParticleSystem = LoadSoftObjectIfAvailable(ImpactParticleSystem))
	{
		if (UWorld* World = GetWorld())
		{
			UParticleSystemComponent* ImpactComponent = UGameplayStatics::SpawnEmitterAtLocation(
				World,
				ParticleSystem,
				ImpactLocation,
				ImpactNormal.Rotation(),
				true);
			if (ImpactComponent)
			{
				if (UMaterialInterface* OverlayMaterial = LoadSoftObjectIfAvailable(FeedbackOverlayMaterial))
				{
					if (UMaterialInstanceDynamic* ImpactMaterial = ImpactComponent->CreateDynamicMaterialInstance(0, OverlayMaterial))
					{
						ImpactMaterial->SetVectorParameterValue(TEXT("TintColor"), GetCueTint(CueTag));
						ImpactMaterial->SetScalarParameterValue(TEXT("OverlayOpacity"), 0.35f);
					}
				}
			}
		}
	}

	const EPRHitReactionStrength ReactionStrength = GetCueReactionStrength(Parameters);
	if (ReactionStrength != EPRHitReactionStrength::None)
	{
		if (USkeletalMeshComponent* Mesh = ResolveSkeletalMesh())
		{
			TSoftObjectPtr<UAnimMontage>& MontageAsset = bLastImpactFromBack
				? BackHitMontage
				: ReactionStrength == EPRHitReactionStrength::Heavy
					? FrontHeavyHitMontage
					: FrontLightHitMontage;
			if (UAnimMontage* Montage = LoadSoftObjectIfAvailable(MontageAsset))
			{
				if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				{
					AnimInstance->Montage_Play(Montage);
				}
			}
		}
	}

	ApplyOverlayTint(GetCueTint(CueTag), 0.10f);
	if (bShouldEmitLocalHitPulse)
	{
		if (APRCharacter* LocalCharacter = Cast<APRCharacter>(OwnerActor); LocalCharacter && LocalCharacter->IsLocallyControlled())
		{
			if (APRPlayerController* Controller = Cast<APRPlayerController>(LocalCharacter->GetController()))
			{
				if (UPRWeaponHUDWidget* WeaponHUD = Controller->GetWeaponHUDWidget())
				{
					FPRResolvedDamage LocalFeedback;
					LocalFeedback.IncomingDirection = IncomingDirection;
					LocalFeedback.HitReactionStrength = ReactionStrength;
					if (CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield
						|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak)
					{
						LocalFeedback.ShieldDamage = FMath::Max(0.0f, Parameters.RawMagnitude);
						LocalFeedback.bShieldBroken = CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak;
					}
					else
					{
						LocalFeedback.HealthDamage = FMath::Max(0.0f, Parameters.RawMagnitude);
						LocalFeedback.bLethal = CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal;
					}
					WeaponHUD->PushLocalHitFeedback(LocalFeedback);
				}

				bool bReduceCameraShake = false;
				if (UGameInstance* GameInstance = Controller->GetGameInstance())
				{
					if (const UPRSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UPRSaveSubsystem>())
					{
						FPRProfileSnapshot ActiveSnapshot;
						if (SaveSubsystem->GetActiveProfileSnapshot(ActiveSnapshot))
						{
							bReduceCameraShake = ActiveSnapshot.Settings.bReduceCameraShake;
						}
					}
				}
				if (APlayerCameraManager* CameraManager = Controller->PlayerCameraManager)
				{
					const float ReactionScale = ReactionStrength == EPRHitReactionStrength::Heavy
						? 1.0f
						: ReactionStrength == EPRHitReactionStrength::Light ? 0.65f : 0.4f;
					CameraManager->StartCameraShake(
						UPRCombatHitCameraShake::StaticClass(),
						ReactionScale * ResolveCameraShakeScale(bReduceCameraShake));
				}
			}
		}
	}
}

void UPRCombatFeedbackComponent::RefreshPersistentOverlay()
{
	if (ActiveStatusCueTags.IsEmpty())
	{
		RestoreOriginalOverlay();
		return;
	}

	FGameplayTag SelectedTag = ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted;
	if (ActiveStatusCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned))
	{
		SelectedTag = ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned;
	}
	else if (ActiveStatusCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed))
	{
		SelectedTag = ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed;
	}
	ApplyOverlayTint(GetCueTint(SelectedTag), 0.0f);
}

void UPRCombatFeedbackComponent::ApplyOverlayTint(
	const FLinearColor& TintColor,
	const float DurationSeconds)
{
	USkeletalMeshComponent* Mesh = ResolveSkeletalMesh();
	if (!Mesh)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = LoadSoftObjectIfAvailable(FeedbackOverlayMaterial);
	if (!BaseMaterial)
	{
		return;
	}

	if (!bHasOverriddenOverlay)
	{
		OriginalOverlayMaterial = Mesh->GetOverlayMaterial();
		bHasOverriddenOverlay = true;
	}
	ActiveOverlayMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!ActiveOverlayMaterial)
	{
		return;
	}
	ActiveOverlayMaterial->SetVectorParameterValue(TEXT("TintColor"), TintColor);
	Mesh->SetOverlayMaterial(ActiveOverlayMaterial);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OverlayTimerHandle);
		if (DurationSeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				OverlayTimerHandle,
				this,
				&UPRCombatFeedbackComponent::RestoreOverlayAfterImpact,
				DurationSeconds,
				false);
		}
	}
}

void UPRCombatFeedbackComponent::RestoreOverlayAfterImpact()
{
	if (ActiveStatusCueTags.IsEmpty())
	{
		RestoreOriginalOverlay();
	}
	else
	{
		RefreshPersistentOverlay();
	}
}

void UPRCombatFeedbackComponent::RestoreOriginalOverlay()
{
	if (!bHasOverriddenOverlay)
	{
		return;
	}
	if (USkeletalMeshComponent* Mesh = ResolveSkeletalMesh())
	{
		Mesh->SetOverlayMaterial(OriginalOverlayMaterial);
	}
	ActiveOverlayMaterial = nullptr;
	OriginalOverlayMaterial = nullptr;
	bHasOverriddenOverlay = false;
}
