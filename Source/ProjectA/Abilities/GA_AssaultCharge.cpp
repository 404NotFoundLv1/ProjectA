#include "Abilities/GA_AssaultCharge.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/PRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/PRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Engine/HitResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectA.h"

UGA_AssaultCharge::UGA_AssaultCharge()
{
	ChargeDistance = 450.0f;
	KnockbackRadius = 160.0f;
	KnockbackStrength = 800.0f;
	TargetStatusEffects.Add(FPRTargetStatusEffectDefinition(
		UPRSlowStatusGameplayEffect::StaticClass(),
		0.70f,
		3.0f));
}

FGameplayTag UGA_AssaultCharge::GetModuleCooldownTag() const
{
	return ProjectRiftGameplayTags::Cooldown_Skill_Q;
}

TSubclassOf<UGameplayEffect> UGA_AssaultCharge::GetModuleCooldownEffectClass() const
{
	return UPRAssaultChargeCooldownGameplayEffect::StaticClass();
}

void UGA_AssaultCharge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!TryCommitAssaultAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ExecuteCharge(ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_AssaultCharge::ExecuteCharge(const FGameplayAbilityActorInfo* ActorInfo) const
{
	APRCharacter* AvatarCharacter = ActorInfo ? Cast<APRCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = AvatarCharacter ? AvatarCharacter->GetWorld() : nullptr;
	if (!AvatarCharacter || !SourceASC || !World)
	{
		return false;
	}

	const FVector StartLocation = AvatarCharacter->GetActorLocation();
	const FVector Forward = AvatarCharacter->GetActorForwardVector().GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return false;
	}
	const FVector IntendedEnd = StartLocation + Forward * ChargeDistance;
	FVector ActualEnd = IntendedEnd;
	FCollisionObjectQueryParams WorldQuery;
	WorldQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	WorldQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams WorldSweepParams(SCENE_QUERY_STAT(PRAssaultChargeWorld), false, AvatarCharacter);
	FHitResult WorldHit;
	const float CapsuleRadius = AvatarCharacter->GetCapsuleComponent() ? AvatarCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
	const float CapsuleHalfHeight = AvatarCharacter->GetCapsuleComponent() ? AvatarCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
	if (World->SweepSingleByObjectType(
		WorldHit,
		StartLocation,
		IntendedEnd,
		FQuat::Identity,
		WorldQuery,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		WorldSweepParams))
	{
		ActualEnd = WorldHit.Location;
	}
	AvatarCharacter->SetActorLocation(ActualEnd, false, nullptr, ETeleportType::TeleportPhysics);
	AvatarCharacter->ForceNetUpdate();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRAssaultCharge), false, AvatarCharacter);
	QueryParams.AddIgnoredActor(AvatarCharacter);

	TArray<FHitResult> PathHits;
	World->SweepMultiByObjectType(
		PathHits,
		StartLocation,
		ActualEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(KnockbackRadius),
		QueryParams);
	TArray<FOverlapResult> EndpointOverlaps;
	World->OverlapMultiByObjectType(
		EndpointOverlaps,
		ActualEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(KnockbackRadius),
		QueryParams);
	TSet<TWeakObjectPtr<AActor>> UniqueTargets;
	for (const FHitResult& PathHit : PathHits)
	{
		if (AActor* TargetActor = PathHit.GetActor())
		{
			UniqueTargets.Add(TargetActor);
		}
	}
	for (const FOverlapResult& EndpointOverlap : EndpointOverlaps)
	{
		if (AActor* TargetActor = EndpointOverlap.GetActor())
		{
			UniqueTargets.Add(TargetActor);
		}
	}

	for (const TWeakObjectPtr<AActor>& WeakTarget : UniqueTargets)
	{
		ACharacter* TargetCharacter = Cast<ACharacter>(WeakTarget.Get());
		if (!TargetCharacter || TargetCharacter == AvatarCharacter)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetCharacter);
		if (!TargetASC || !ApplyConfiguredStatusEffects(SourceASC, TargetASC))
		{
			continue;
		}

		if (UCharacterMovementComponent* MovementComponent = TargetCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		TargetCharacter->LaunchCharacter(Forward * KnockbackStrength, true, true);
	}

	UE_LOG(LogProjectA, Log, TEXT("Assault charge moved %s %.0f units."), *GetNameSafe(AvatarCharacter), FVector::Distance(StartLocation, ActualEnd));
	return true;
}
