#include "Abilities/GA_AssaultBlast.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRDamageGameplayEffect.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "GameplayEffect.h"
#include "ProjectA.h"

UGA_AssaultBlast::UGA_AssaultBlast()
{
	EnergyCost = 25.0f;
	CooldownSeconds = 1.25f;
	BlastRange = 360.0f;
	BlastRadius = 180.0f;
	DamageAmount = 25.0f;
	DamageEffectClass = UPRDamageGameplayEffect::StaticClass();
	TargetStatusEffects.Add(FPRTargetStatusEffectDefinition(
		UPRStunStatusGameplayEffect::StaticClass(),
		0.0f,
		1.25f));
}

void UGA_AssaultBlast::ActivateAbility(
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
		ExecuteBlast(Handle, ActorInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_AssaultBlast::ExecuteBlast(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !SourceASC || !World || !DamageEffectClass)
	{
		return false;
	}

	const FVector Forward = AvatarActor->GetActorForwardVector();
	const FVector BlastCenter = AvatarActor->GetActorLocation() + Forward * (BlastRange * 0.5f);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRAssaultBlast), false, AvatarActor);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> Overlaps;
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		Overlaps,
		BlastCenter,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(BlastRadius),
		QueryParams);
	if (!bHasOverlap)
	{
		return false;
	}

	bool bDamagedAnyTarget = false;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!TargetActor || TargetActor == AvatarActor)
		{
			continue;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
		if (FVector::DotProduct(Forward, ToTarget.GetSafeNormal()) < 0.1f)
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!TargetASC)
		{
			continue;
		}

		if (!UPRCombatEffectLibrary::ApplyDamageToTarget(
			SourceASC,
			TargetASC,
			DamageAmount,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			const_cast<UGA_AssaultBlast*>(this)))
		{
			continue;
		}

		ApplyConfiguredStatusEffects(SourceASC, TargetASC);
		bDamagedAnyTarget = true;
	}

	UE_LOG(LogProjectA, Log, TEXT("Assault blast from %s damaged target: %s."), *GetNameSafe(AvatarActor), bDamagedAnyTarget ? TEXT("true") : TEXT("false"));
	return bDamagedAnyTarget;
}
