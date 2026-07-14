#include "Abilities/GA_AssaultCharge.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/PRCharacter.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ProjectA.h"

UGA_AssaultCharge::UGA_AssaultCharge()
{
	EnergyCost = 15.0f;
	CooldownSeconds = 1.0f;
	ChargeDistance = 450.0f;
	KnockbackRadius = 160.0f;
	KnockbackStrength = 800.0f;
	TargetStatusEffects.Add(FPRTargetStatusEffectDefinition(
		UPRSlowStatusGameplayEffect::StaticClass(),
		0.70f,
		3.0f));
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

	const FVector Forward = AvatarCharacter->GetActorForwardVector();
	const FVector TargetLocation = AvatarCharacter->GetActorLocation() + Forward * ChargeDistance;
	AvatarCharacter->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
	AvatarCharacter->ForceNetUpdate();

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRAssaultCharge), false, AvatarCharacter);
	QueryParams.AddIgnoredActor(AvatarCharacter);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		AvatarCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(KnockbackRadius),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* TargetCharacter = Cast<ACharacter>(Overlap.GetActor());
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

	UE_LOG(LogProjectA, Log, TEXT("Assault charge moved %s."), *GetNameSafe(AvatarCharacter));
	return true;
}
