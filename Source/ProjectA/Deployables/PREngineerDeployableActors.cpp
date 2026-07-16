#include "Deployables/PREngineerDeployableActors.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Enemies/PREnemyCharacter.h"
#include "EngineUtils.h"
#include "Characters/PRCharacter.h"
#include "Components/TextRenderComponent.h"
#include "Core/PRGameplayTags.h"
#include "Player/PRPlayerState.h"
#include "Roles/PREngineerModuleDataAsset.h"
#include "TimerManager.h"

namespace
{
UAbilitySystemComponent* GetOwnerASC(const APRDeployableActor* Deployable)
{
	const APRPlayerState* PlayerState = Deployable ? Deployable->GetOwningPlayerState() : nullptr;
	return PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
}

bool HasLineOfSight(const AActor* Source, const AActor* Target)
{
	if (!Source || !Target || !Source->GetWorld()) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PRDeployableEffectLOS), false, Source);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !Source->GetWorld()->LineTraceSingleByChannel(Hit, Source->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}
}

void APRDeployableActor::ConfigureFromModule(const UPREngineerModuleDataAsset* Module)
{
}

void APRSentryDeployable::ConfigureFromModule(const UPREngineerModuleDataAsset* Module)
{
	if (!HasAuthority() || !Module) return;
	EffectRange = Module->EffectRange;
	DamageAmount = Module->PrimaryMagnitude;
	LabelComponent->SetText(FText::FromString(TEXT("SENTRY")));
	GetWorldTimerManager().SetTimer(EffectTimer, this, &APRSentryDeployable::FireAtNearestEnemy, Module->EffectIntervalSeconds, true, 0.0f);
}

void APRSentryDeployable::FireAtNearestEnemy()
{
	UAbilitySystemComponent* SourceASC = GetOwnerASC(this);
	APREnemyCharacter* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (TActorIterator<APREnemyCharacter> It(GetWorld()); It; ++It)
	{
		APREnemyCharacter* Candidate = *It;
		const float DistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq > FMath::Square(EffectRange) || DistanceSq >= BestDistanceSq || !HasLineOfSight(this, Candidate)) continue;
		if (UAbilitySystemComponent* TargetASC = Candidate->GetAbilitySystemComponent(); UPRCombatEffectLibrary::IsHostileTarget(SourceASC, TargetASC))
		{
			BestTarget = Candidate;
			BestDistanceSq = DistanceSq;
		}
	}
	if (!BestTarget || !SourceASC) return;
	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactPoint = BestTarget->GetActorLocation();
	Hit.Location = Hit.ImpactPoint;
	Hit.ImpactNormal = (GetActorLocation() - Hit.ImpactPoint).GetSafeNormal();
	FPRDamageRequest Request;
	Request.BaseDamage = DamageAmount;
	Request.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	Request.HitResult = Hit;
	Request.HitReaction = { EPRHitReactionStrength::Light, 0.08f };
	Request.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
	UPRCombatEffectLibrary::ApplyDamageRequestToTarget(SourceASC, BestTarget->GetAbilitySystemComponent(), Request, this);
}

void APRRepairDroneDeployable::ConfigureFromModule(const UPREngineerModuleDataAsset* Module)
{
	if (!HasAuthority() || !Module) return;
	EffectRange = Module->EffectRange;
	RepairAmount = Module->PrimaryMagnitude;
	LabelComponent->SetText(FText::FromString(TEXT("REPAIR")));
	GetWorldTimerManager().SetTimer(EffectTimer, this, &APRRepairDroneDeployable::RepairMostDamagedFriendly, Module->EffectIntervalSeconds, true, 0.0f);
}

void APRRepairDroneDeployable::RepairMostDamagedFriendly()
{
	UAbilitySystemComponent* SourceASC = GetOwnerASC(this);
	APRCharacter* BestTarget = nullptr;
	float BestMissingRatio = 0.0f;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (TActorIterator<APRCharacter> It(GetWorld()); It; ++It)
	{
		APRCharacter* Candidate = *It;
		UAbilitySystemComponent* TargetASC = Candidate->GetAbilitySystemComponent();
		const UPRAttributeSet* Attributes = TargetASC ? TargetASC->GetSet<UPRAttributeSet>() : nullptr;
		const float DistanceSq = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (!Attributes || DistanceSq > FMath::Square(EffectRange) || !UPRCombatEffectLibrary::IsFriendlyPlayerTarget(SourceASC, TargetASC)) continue;
		const float MaxShield = Attributes->GetMaxShield();
		const float MissingRatio = MaxShield > 0.0f ? FMath::Clamp((MaxShield - Attributes->GetShield()) / MaxShield, 0.0f, 1.0f) : 0.0f;
		if (MissingRatio <= 0.0f) continue;
		if (!BestTarget || MissingRatio > BestMissingRatio || (FMath::IsNearlyEqual(MissingRatio, BestMissingRatio) && (DistanceSq < BestDistanceSq || (FMath::IsNearlyEqual(DistanceSq, BestDistanceSq) && Candidate->GetUniqueID() < BestTarget->GetUniqueID()))))
		{
			BestTarget = Candidate; BestMissingRatio = MissingRatio; BestDistanceSq = DistanceSq;
		}
	}
	if (BestTarget && SourceASC)
	{
		UPRCombatEffectLibrary::ApplyShieldRepairToTarget(SourceASC, BestTarget->GetAbilitySystemComponent(), RepairAmount, this);
	}
}

void APRShieldGeneratorDeployable::ConfigureFromModule(const UPREngineerModuleDataAsset* Module)
{
	if (!HasAuthority() || !Module) return;
	EffectRange = Module->EffectRange;
	LabelComponent->SetText(FText::FromString(TEXT("SHIELD")));
	GetWorldTimerManager().SetTimer(EffectTimer, this, &APRShieldGeneratorDeployable::RefreshFriendlyAura, 0.25f, true, 0.0f);
}

void APRShieldGeneratorDeployable::RefreshFriendlyAura()
{
	UAbilitySystemComponent* SourceASC = GetOwnerASC(this);
	if (!SourceASC) return;
	for (TActorIterator<APRCharacter> It(GetWorld()); It; ++It)
	{
		APRCharacter* Candidate = *It;
		if (FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation()) <= FMath::Square(EffectRange))
		{
			UPRCombatEffectLibrary::ApplyShieldGeneratorAuraToTarget(SourceASC, Candidate->GetAbilitySystemComponent(), this);
		}
	}
}
