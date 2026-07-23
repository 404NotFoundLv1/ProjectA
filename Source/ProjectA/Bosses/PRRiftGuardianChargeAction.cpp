#include "Bosses/PRRiftGuardianChargeAction.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Bosses/PRBossCharacter.h"
#include "Characters/PRCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Core/PRGameplayTags.h"
#include "Engine/World.h"

APRRiftGuardianChargeAction::APRRiftGuardianChargeAction()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetReplicates(false);
}

void APRRiftGuardianChargeAction::InitializeCharge(
	APRBossCharacter* InSourceBoss,
	const FVector& InLockedTargetLocation,
	const FPRBossAbilityPatternDefinition& Pattern)
{
	SourceBoss = InSourceBoss;
	if (!InSourceBoss)
	{
		Destroy();
		return;
	}
	StartLocation = InSourceBoss->GetActorLocation();
	FVector ToTarget = InLockedTargetLocation - StartLocation;
	ToTarget.Z = 0.0f;
	ChargeDistance = FMath::Min(2400.0f, ToTarget.Size());
	ChargeDirection = ToTarget.IsNearlyZero() ? InSourceBoss->GetActorForwardVector() : ToTarget.GetSafeNormal();
	ChargeSpeed = 1400.0f;
	DamageRadius = FMath::Max(1.0f, Pattern.EffectRadius > 0.0f ? Pattern.EffectRadius : 180.0f);
	BaseDamage = FMath::Max(0.0f, Pattern.BaseDamage);
	DamageType = Pattern.DamageType.IsValid() ? Pattern.DamageType : ProjectRiftGameplayTags::Damage_Type_Physical;
	HitReaction = Pattern.HitReaction;
	SetActorLocation(StartLocation);
	if (ChargeDistance <= KINDA_SMALL_NUMBER)
	{
		Destroy();
	}
}

void APRRiftGuardianChargeAction::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APRBossCharacter* Boss = SourceBoss.Get();
	UWorld* World = GetWorld();
	if (!Boss || !Boss->HasAuthority() || !World || DeltaSeconds <= 0.0f)
	{
		Destroy();
		return;
	}

	const float Step = FMath::Min(ChargeSpeed * DeltaSeconds, ChargeDistance - TravelledDistance);
	if (Step <= KINDA_SMALL_NUMBER)
	{
		Destroy();
		return;
	}
	const FVector PreviousLocation = Boss->GetActorLocation();
	const FVector NextLocation = PreviousLocation + ChargeDirection * Step;
	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RiftGuardianCharge), false, Boss);
	World->SweepMultiByChannel(Hits, PreviousLocation, NextLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(DamageRadius), QueryParams);
	ApplyChargeDamage(Hits);

	FHitResult BlockingHit;
	Boss->SetActorLocation(NextLocation, true, &BlockingHit, ETeleportType::None);
	TravelledDistance += Step;
	if (BlockingHit.bBlockingHit || TravelledDistance >= ChargeDistance - KINDA_SMALL_NUMBER)
	{
		Destroy();
	}
}

void APRRiftGuardianChargeAction::ApplyChargeDamage(const TArray<FHitResult>& Hits)
{
	APRBossCharacter* Boss = SourceBoss.Get();
	UPRAbilitySystemComponent* SourceASC = Boss ? Boss->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!Boss || !SourceASC || BaseDamage <= 0.0f)
	{
		return;
	}
	for (const FHitResult& Hit : Hits)
	{
		APRCharacter* Target = Cast<APRCharacter>(Hit.GetActor());
		if (!Target || !Target->IsAlive() || DamagedActors.Contains(TObjectKey<AActor>(Target)))
		{
			continue;
		}
		UPRAbilitySystemComponent* TargetASC = Target->GetProjectRiftAbilitySystemComponent();
		if (!TargetASC)
		{
			continue;
		}
		FPRDamageRequest Request;
		Request.BaseDamage = BaseDamage;
		Request.DamageType = DamageType;
		Request.HitResult = Hit;
		Request.HitReaction = HitReaction;
		Request.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
		if (UPRCombatEffectLibrary::ApplyDamageRequestToTarget(SourceASC, TargetASC, Request, Boss))
		{
			DamagedActors.Add(TObjectKey<AActor>(Target));
		}
	}
}
