#include "Abilities/GA_PrimaryAttack.h"

#include "Abilities/PRDamageGameplayEffect.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Enemies/PREnemyCharacter.h"
#include "Engine/OverlapResult.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "ProjectA.h"

UGA_PrimaryAttack::UGA_PrimaryAttack()
{
	AttackRange = 240.0f;
	AttackRadius = 110.0f;
	DamageAmount = 10.0f;
	DamageEffectClass = UPRDamageGameplayEffect::StaticClass();

	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DeadStateTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DeadStateTag);
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	if (DownedStateTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DownedStateTag);
	}
}

bool UGA_PrimaryAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DeadStateTag.IsValid() && AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(DeadStateTag))
	{
		return false;
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	return !DownedStateTag.IsValid() || !AbilitySystemComponent || !AbilitySystemComponent->HasMatchingGameplayTag(DownedStateTag);
}

void UGA_PrimaryAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		ExecuteServerAttack(Handle, ActorInfo, ActivationInfo);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UGA_PrimaryAttack::ExecuteServerAttack(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !SourceASC || !World || !DamageEffectClass)
	{
		return false;
	}

	const FVector Forward = AvatarActor->GetActorForwardVector();
	const FVector TraceCenter = AvatarActor->GetActorLocation() + Forward * (AttackRange * 0.5f);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRPrimaryAttack), false, AvatarActor);
	QueryParams.AddIgnoredActor(AvatarActor);

	TArray<FOverlapResult> Overlaps;
	const bool bHasHit = World->OverlapMultiByObjectType(
		Overlaps,
		TraceCenter,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(AttackRadius),
		QueryParams);

	if (!bHasHit)
	{
		UE_LOG(LogProjectA, Log, TEXT("Primary attack from %s found no target."), *GetNameSafe(AvatarActor));
		return false;
	}

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
			const_cast<UGA_PrimaryAttack*>(this)))
		{
			continue;
		}

		ApplyConfiguredStatusEffects(SourceASC, TargetASC);
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Primary attack from %s hit %s."),
			*GetNameSafe(AvatarActor),
			*GetNameSafe(TargetActor));
		return true;
	}

	UE_LOG(LogProjectA, Log, TEXT("Primary attack from %s found no valid target."), *GetNameSafe(AvatarActor));
	return false;
}
