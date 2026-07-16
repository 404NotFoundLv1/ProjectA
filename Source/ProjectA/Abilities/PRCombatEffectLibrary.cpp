#include "Abilities/PRCombatEffectLibrary.h"

#include "Abilities/PRDamageGameplayEffect.h"
#include "Abilities/PRHitStaggerGameplayEffect.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Abilities/PRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Combat/PRCombatUnitInterface.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"

namespace
{
enum class EPRCombatantKind : uint8
{
	Unknown,
	Player,
	Enemy
};

AActor* ResolveCombatActor(const UAbilitySystemComponent* AbilitySystem)
{
	if (!AbilitySystem)
	{
		return nullptr;
	}

	return AbilitySystem->GetAvatarActor()
		? AbilitySystem->GetAvatarActor()
		: AbilitySystem->GetOwnerActor();
}

EPRCombatantKind GetCombatantKind(const UAbilitySystemComponent* AbilitySystem)
{
	const AActor* CombatActor = ResolveCombatActor(AbilitySystem);
	if (Cast<APRCharacter>(CombatActor) || Cast<APRPlayerState>(CombatActor))
	{
		return EPRCombatantKind::Player;
	}
	if (Cast<APREnemyCharacter>(CombatActor))
	{
		return EPRCombatantKind::Enemy;
	}

	return EPRCombatantKind::Unknown;
}

bool HasInactiveState(const UAbilitySystemComponent* AbilitySystem)
{
	if (!AbilitySystem)
	{
		return true;
	}

	if (AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
		|| AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Downed))
	{
		return true;
	}

	const IPRCombatUnitInterface* CombatUnit = Cast<IPRCombatUnitInterface>(ResolveCombatActor(AbilitySystem));
	return CombatUnit && CombatUnit->IsCombatUnitInactive();
}

bool HasExplicitInactiveState(const UAbilitySystemComponent* AbilitySystem)
{
	return !AbilitySystem
		|| AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
		|| AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Downed);
}

bool AreOpposingCombatants(
	const UAbilitySystemComponent* SourceAbilitySystem,
	const UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!SourceAbilitySystem || !TargetAbilitySystem || SourceAbilitySystem == TargetAbilitySystem)
	{
		return false;
	}

	const EPRCombatantKind SourceKind = GetCombatantKind(SourceAbilitySystem);
	const EPRCombatantKind TargetKind = GetCombatantKind(TargetAbilitySystem);
	return (SourceKind == EPRCombatantKind::Player && TargetKind == EPRCombatantKind::Enemy)
		|| (SourceKind == EPRCombatantKind::Enemy && TargetKind == EPRCombatantKind::Player);
}

bool HasAuthoritativeActorInfo(const UAbilitySystemComponent* AbilitySystem)
{
	if (!AbilitySystem)
	{
		return false;
	}

	const AActor* OwnerActor = AbilitySystem->GetOwnerActor();
	const AActor* AvatarActor = AbilitySystem->GetAvatarActor();
	return OwnerActor
		&& AvatarActor
		&& OwnerActor->HasAuthority()
		&& AvatarActor->HasAuthority();
}

bool IsValidFeedbackPolicy(const EPRCombatFeedbackPolicy Policy)
{
	return Policy == EPRCombatFeedbackPolicy::None
		|| Policy == EPRCombatFeedbackPolicy::TargetOnly
		|| Policy == EPRCombatFeedbackPolicy::TargetAndSource;
}

bool IsValidHitReactionStrength(const EPRHitReactionStrength Strength)
{
	return Strength == EPRHitReactionStrength::None
		|| Strength == EPRHitReactionStrength::Light
		|| Strength == EPRHitReactionStrength::Heavy;
}

bool IsFiniteBlockingHitContext(const FHitResult& HitResult)
{
	if (!HitResult.IsValidBlockingHit())
	{
		return true;
	}

	return !FVector(HitResult.Location).ContainsNaN()
		&& !FVector(HitResult.ImpactPoint).ContainsNaN()
		&& !FVector(HitResult.Normal).ContainsNaN()
		&& !FVector(HitResult.ImpactNormal).ContainsNaN()
		&& !FVector(HitResult.TraceStart).ContainsNaN()
		&& !FVector(HitResult.TraceEnd).ContainsNaN()
		&& FMath::IsFinite(HitResult.Time)
		&& FMath::IsFinite(HitResult.Distance)
		&& FMath::IsFinite(HitResult.PenetrationDepth);
}

EPRHitReactionStrength DecodeHitReactionStrength(const float EncodedStrength)
{
	if (FMath::IsNearlyEqual(EncodedStrength, static_cast<float>(EPRHitReactionStrength::Light)))
	{
		return EPRHitReactionStrength::Light;
	}
	if (FMath::IsNearlyEqual(EncodedStrength, static_cast<float>(EPRHitReactionStrength::Heavy)))
	{
		return EPRHitReactionStrength::Heavy;
	}
	return EPRHitReactionStrength::None;
}

struct FPRActiveStaggerState
{
	TArray<FActiveGameplayEffectHandle> Handles;
	EPRHitReactionStrength StrongestStrength = EPRHitReactionStrength::None;
};

FPRActiveStaggerState FindActiveHitStaggers(const UAbilitySystemComponent* TargetAbilitySystem)
{
	FPRActiveStaggerState Result;
	if (!TargetAbilitySystem)
	{
		return Result;
	}

	FGameplayTagContainer HitStaggerTags;
	HitStaggerTags.AddTag(ProjectRiftGameplayTags::State_HitStaggered);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(HitStaggerTags);
	for (const FActiveGameplayEffectHandle Handle : TargetAbilitySystem->GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* ActiveEffect = TargetAbilitySystem->GetActiveGameplayEffect(Handle);
		if (!ActiveEffect)
		{
			continue;
		}

		Result.Handles.Add(Handle);
		const EPRHitReactionStrength Strength = DecodeHitReactionStrength(
			ActiveEffect->Spec.GetSetByCallerMagnitude(
				ProjectRiftGameplayTags::Data_HitReaction_Strength,
				false,
				0.0f));
		if (static_cast<uint8>(Strength) > static_cast<uint8>(Result.StrongestStrength))
		{
			Result.StrongestStrength = Strength;
		}
	}
	return Result;
}

FPRDamageRequest MakeFailClosedDamageRequest(const FPRDamageRequest& DamageRequest)
{
	FPRDamageRequest SanitizedRequest = DamageRequest;
	if (!IsValidFeedbackPolicy(DamageRequest.FeedbackPolicy)
		|| !IsValidHitReactionStrength(DamageRequest.HitReaction.Strength))
	{
		SanitizedRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::None;
		SanitizedRequest.HitReaction.Strength = EPRHitReactionStrength::None;
		SanitizedRequest.HitReaction.DurationSeconds = 0.0f;
	}
	return SanitizedRequest;
}

FGameplayEffectContextHandle MakeCombatEffectContext(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	UObject* SourceObject)
{
	FGameplayEffectContextHandle Context = SourceAbilitySystem->MakeEffectContext();
	AActor* SourceActor = ResolveCombatActor(SourceAbilitySystem);
	AActor* TargetActor = ResolveCombatActor(TargetAbilitySystem);
	Context.AddSourceObject(SourceObject ? SourceObject : SourceActor);
	if (SourceActor)
	{
		Context.AddInstigator(SourceActor, SourceActor);
	}
	if (TargetActor)
	{
		Context.AddActors({ TargetActor }, true);
	}
	return Context;
}

UPRCombatFeedbackComponent* ResolveFeedbackComponent(const UAbilitySystemComponent* AbilitySystem)
{
	AActor* CombatActor = ResolveCombatActor(AbilitySystem);
	return CombatActor ? CombatActor->FindComponentByClass<UPRCombatFeedbackComponent>() : nullptr;
}

void ClearHitStagger(UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!TargetAbilitySystem)
	{
		return;
	}

	FGameplayTagContainer HitStaggerTags;
	HitStaggerTags.AddTag(ProjectRiftGameplayTags::State_HitStaggered);
	TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(HitStaggerTags);
	if (UPRCombatFeedbackComponent* FeedbackComponent = ResolveFeedbackComponent(TargetAbilitySystem))
	{
		FeedbackComponent->ClearActiveStagger();
	}
}
}

bool UPRCombatEffectLibrary::IsHostileTarget(
	const UAbilitySystemComponent* SourceAbilitySystem,
	const UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!SourceAbilitySystem || !TargetAbilitySystem || SourceAbilitySystem == TargetAbilitySystem || HasInactiveState(TargetAbilitySystem))
	{
		return false;
	}

	return AreOpposingCombatants(SourceAbilitySystem, TargetAbilitySystem);
}

bool UPRCombatEffectLibrary::IsFriendlyPlayerTarget(
	const UAbilitySystemComponent* SourceAbilitySystem,
	const UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!SourceAbilitySystem || !TargetAbilitySystem || HasInactiveState(TargetAbilitySystem))
	{
		return false;
	}

	return GetCombatantKind(SourceAbilitySystem) == EPRCombatantKind::Player
		&& GetCombatantKind(TargetAbilitySystem) == EPRCombatantKind::Player;
}

bool UPRCombatEffectLibrary::ApplyShieldRepairToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const float BaseRepair,
	UObject* SourceObject)
{
	if (!HasAuthoritativeActorInfo(SourceAbilitySystem)
		|| !HasAuthoritativeActorInfo(TargetAbilitySystem)
		|| !IsFriendlyPlayerTarget(SourceAbilitySystem, TargetAbilitySystem)
		|| !FMath::IsFinite(BaseRepair) || BaseRepair <= 0.0f)
	{
		return false;
	}
	const UPRAttributeSet* SourceAttributes = SourceAbilitySystem->GetSet<UPRAttributeSet>();
	const UPRAttributeSet* TargetAttributes = TargetAbilitySystem->GetSet<UPRAttributeSet>();
	if (!SourceAttributes || !TargetAttributes || TargetAttributes->GetShield() >= TargetAttributes->GetMaxShield())
	{
		return false;
	}
	const float RepairAmount = BaseRepair * (1.0f + FMath::Max(0.0f, SourceAttributes->GetHealingPower()) / 100.0f);
	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		UPRShieldRepairGameplayEffect::StaticClass(), 1.0f,
		MakeCombatEffectContext(SourceAbilitySystem, TargetAbilitySystem, SourceObject));
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_ShieldRepair, RepairAmount);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystem);
	return true;
}

bool UPRCombatEffectLibrary::ApplyHealthHealingToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const float BaseHealing,
	UObject* SourceObject)
{
	if (!HasAuthoritativeActorInfo(SourceAbilitySystem)
		|| !HasAuthoritativeActorInfo(TargetAbilitySystem)
		|| !IsFriendlyPlayerTarget(SourceAbilitySystem, TargetAbilitySystem)
		|| !FMath::IsFinite(BaseHealing) || BaseHealing <= 0.0f)
	{
		return false;
	}
	const UPRAttributeSet* SourceAttributes = SourceAbilitySystem->GetSet<UPRAttributeSet>();
	const UPRAttributeSet* TargetAttributes = TargetAbilitySystem->GetSet<UPRAttributeSet>();
	if (!SourceAttributes || !TargetAttributes || TargetAttributes->GetHealth() >= TargetAttributes->GetMaxHealth())
	{
		return false;
	}
	const float HealingAmount = BaseHealing * (1.0f + FMath::Max(0.0f, SourceAttributes->GetHealingPower()) / 100.0f);
	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		UPRHealthHealingGameplayEffect::StaticClass(),
		1.0f,
		MakeCombatEffectContext(SourceAbilitySystem, TargetAbilitySystem, SourceObject));
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Healing, HealingAmount);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystem);
	return true;
}

bool UPRCombatEffectLibrary::ApplyShieldGeneratorAuraToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	UObject* SourceObject)
{
	if (!HasAuthoritativeActorInfo(SourceAbilitySystem)
		|| !HasAuthoritativeActorInfo(TargetAbilitySystem)
		|| !IsFriendlyPlayerTarget(SourceAbilitySystem, TargetAbilitySystem))
	{
		return false;
	}
	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		UPRShieldGeneratorAuraGameplayEffect::StaticClass(), 1.0f,
		MakeCombatEffectContext(SourceAbilitySystem, TargetAbilitySystem, SourceObject));
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystem);
	return true;
}

bool UPRCombatEffectLibrary::ApplyDamageToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const float BaseDamage,
	FGameplayTag DamageType,
	UObject* SourceObject)
{
	if (!IsHostileTarget(SourceAbilitySystem, TargetAbilitySystem)
		|| !FMath::IsFinite(BaseDamage)
		|| BaseDamage <= 0.0f)
	{
		return false;
	}

	if (!DamageType.IsValid())
	{
		DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	}

	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		UPRDamageGameplayEffect::StaticClass(),
		1.0f,
		MakeCombatEffectContext(SourceAbilitySystem, TargetAbilitySystem, SourceObject));
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Damage, BaseDamage);
	SpecHandle.Data->AddDynamicAssetTag(DamageType);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystem);
	return true;
}

bool UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FPRDamageRequest& DamageRequest,
	UObject* SourceObject)
{
	if (!HasAuthoritativeActorInfo(SourceAbilitySystem)
		|| !HasAuthoritativeActorInfo(TargetAbilitySystem)
		|| !IsHostileTarget(SourceAbilitySystem, TargetAbilitySystem)
		|| !FMath::IsFinite(DamageRequest.BaseDamage)
		|| DamageRequest.BaseDamage <= 0.0f
		|| !IsFiniteBlockingHitContext(DamageRequest.HitResult))
	{
		return false;
	}

	const FPRDamageRequest SanitizedRequest = MakeFailClosedDamageRequest(DamageRequest);
	FGameplayTag DamageType = SanitizedRequest.DamageType;
	if (!DamageType.IsValid())
	{
		DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	}

	FGameplayEffectContextHandle EffectContext = MakeCombatEffectContext(
		SourceAbilitySystem,
		TargetAbilitySystem,
		SourceObject);
	if (SanitizedRequest.HitResult.IsValidBlockingHit())
	{
		EffectContext.AddHitResult(SanitizedRequest.HitResult, true);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		UPRDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Damage, SanitizedRequest.BaseDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_CombatFeedback_Policy,
		static_cast<float>(SanitizedRequest.FeedbackPolicy));
	SpecHandle.Data->SetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_HitReaction_Strength,
		static_cast<float>(SanitizedRequest.HitReaction.Strength));
	SpecHandle.Data->SetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_HitReaction_Duration,
		SanitizedRequest.HitReaction.DurationSeconds);
	SpecHandle.Data->AddDynamicAssetTag(ProjectRiftGameplayTags::Data_CombatFeedback_Request);
	SpecHandle.Data->AddDynamicAssetTag(DamageType);
	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystem);
	return true;
}

void UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FPRDamageRequest& DamageRequest,
	const FPRResolvedDamage& ResolvedDamage,
	const FGameplayEffectContextHandle& EffectContext)
{
	DispatchResolvedDamageFeedbackInternal(
		SourceAbilitySystem,
		TargetAbilitySystem,
		DamageRequest,
		ResolvedDamage,
		EffectContext,
		false);
}

void UPRCombatEffectLibrary::DispatchResolvedDamageFeedbackInternal(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FPRDamageRequest& DamageRequest,
	const FPRResolvedDamage& ResolvedDamage,
	const FGameplayEffectContextHandle& EffectContext,
	const bool bAllowPendingLethalTransition)
{
	if (!HasAuthoritativeActorInfo(SourceAbilitySystem)
		|| !HasAuthoritativeActorInfo(TargetAbilitySystem)
		|| DamageRequest.DamageType != ProjectRiftGameplayTags::Damage_Type_Physical)
	{
		return;
	}

	const FPRDamageRequest SanitizedRequest = MakeFailClosedDamageRequest(DamageRequest);
	const float ActualDamage = ResolvedDamage.ShieldDamage + ResolvedDamage.HealthDamage;
	const bool bHasActualDamage = FMath::IsFinite(ResolvedDamage.ShieldDamage)
		&& FMath::IsFinite(ResolvedDamage.HealthDamage)
		&& FMath::IsFinite(ActualDamage)
		&& ResolvedDamage.ShieldDamage >= 0.0f
		&& ResolvedDamage.HealthDamage >= 0.0f
		&& ActualDamage > 0.0f;
	if (!bHasActualDamage
		|| !AreOpposingCombatants(SourceAbilitySystem, TargetAbilitySystem)
		|| (HasExplicitInactiveState(TargetAbilitySystem)
			&& !(bAllowPendingLethalTransition && ResolvedDamage.bLethal)))
	{
		return;
	}

	UPRCombatFeedbackComponent* FeedbackComponent = ResolveFeedbackComponent(TargetAbilitySystem);
	if (SanitizedRequest.FeedbackPolicy == EPRCombatFeedbackPolicy::TargetAndSource)
	{
		AActor* SourceAvatar = SourceAbilitySystem->GetAvatarActor();
		APawn* SourcePawn = Cast<APRCharacter>(SourceAvatar);
		if (!SourcePawn)
		{
			SourcePawn = Cast<APawn>(SourceAvatar);
		}
		if (APRPlayerController* SourceController = SourcePawn
			? Cast<APRPlayerController>(SourcePawn->GetController())
			: nullptr;
			SourceController && SourceController->HasAuthority())
		{
			FPRHitConfirmation Confirmation;
			Confirmation.ShieldDamage = ResolvedDamage.ShieldDamage;
			Confirmation.HealthDamage = ResolvedDamage.HealthDamage;
			Confirmation.bShieldBroken = ResolvedDamage.bShieldBroken;
			Confirmation.bLethal = ResolvedDamage.bLethal;
			SourceController->SendHitConfirmationToOwner(Confirmation);
		}
	}
	if (FeedbackComponent
		&& (SanitizedRequest.FeedbackPolicy == EPRCombatFeedbackPolicy::TargetOnly
		|| SanitizedRequest.FeedbackPolicy == EPRCombatFeedbackPolicy::TargetAndSource)
		)
	{
		FeedbackComponent->RecordResolvedDamage(ResolvedDamage, EffectContext);
		const int32 FeedbackSequence = FeedbackComponent->LastDamageFeedbackSequence;
		for (const FGameplayTag CueTag : FeedbackComponent->LastDamageCueTags)
		{
			FGameplayCueParameters CueParameters(EffectContext);
			CueParameters.EffectContext = EffectContext;
			CueParameters.Instigator = EffectContext.GetOriginalInstigator();
			CueParameters.EffectCauser = EffectContext.GetEffectCauser();
			CueParameters.SourceObject = EffectContext.GetSourceObject();
			CueParameters.AbilityLevel = FeedbackSequence;
			CueParameters.NormalizedMagnitude = FMath::Clamp(
				static_cast<float>(ResolvedDamage.HitReactionStrength) / static_cast<float>(EPRHitReactionStrength::Heavy),
				0.0f,
				1.0f);
			CueParameters.RawMagnitude = CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield
				|| CueTag == ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak
				? ResolvedDamage.ShieldDamage
				: ResolvedDamage.HealthDamage;
			if (const FHitResult* HitResult = EffectContext.GetHitResult())
			{
				CueParameters.Location = HitResult->ImpactPoint;
				CueParameters.Normal = HitResult->ImpactNormal;
			}
			TargetAbilitySystem->ExecuteGameplayCue(CueTag, CueParameters);
		}
	}

	const float StaggerDuration = SanitizedRequest.HitReaction.DurationSeconds;
	const EPRHitReactionStrength RequestedStrength = SanitizedRequest.HitReaction.Strength;
	if (ResolvedDamage.bLethal
		|| HasInactiveState(TargetAbilitySystem)
		|| TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned)
		|| RequestedStrength == EPRHitReactionStrength::None
		|| !FMath::IsFinite(StaggerDuration)
		|| StaggerDuration <= 0.0f)
	{
		return;
	}

	FPRActiveStaggerState ExistingStaggers = FindActiveHitStaggers(TargetAbilitySystem);
	if (ExistingStaggers.Handles.IsEmpty() && FeedbackComponent)
	{
		const FActiveGameplayEffectHandle CachedHandle = FeedbackComponent->GetActiveStaggerHandle();
		if (CachedHandle.IsValid() && TargetAbilitySystem->GetActiveGameplayEffect(CachedHandle))
		{
			ExistingStaggers.Handles.Add(CachedHandle);
			ExistingStaggers.StrongestStrength = FeedbackComponent->GetActiveStaggerStrength();
		}
	}
	if (!ExistingStaggers.Handles.IsEmpty()
		&& static_cast<uint8>(RequestedStrength)
			< static_cast<uint8>(ExistingStaggers.StrongestStrength))
	{
		return;
	}

	for (const FActiveGameplayEffectHandle ExistingHandle : ExistingStaggers.Handles)
	{
		TargetAbilitySystem->RemoveActiveGameplayEffect(ExistingHandle);
	}
	if (FeedbackComponent)
	{
		FeedbackComponent->ClearActiveStagger();
	}

	FGameplayEffectSpecHandle StaggerSpec = SourceAbilitySystem->MakeOutgoingSpec(
		UPRHitStaggerGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!StaggerSpec.IsValid())
	{
		return;
	}

	StaggerSpec.Data->SetDuration(StaggerDuration, true);
	StaggerSpec.Data->SetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_HitReaction_Strength,
		static_cast<float>(RequestedStrength));
	const FActiveGameplayEffectHandle StaggerHandle = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
		*StaggerSpec.Data.Get(),
		TargetAbilitySystem);
	if (StaggerHandle.IsValid())
	{
		if (FeedbackComponent)
		{
			FeedbackComponent->SetActiveStagger(StaggerHandle, RequestedStrength);
		}
		TargetAbilitySystem->CancelAllAbilities();
	}
}

FActiveGameplayEffectHandle UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FPRTargetStatusEffectDefinition& Definition,
	UObject* SourceObject)
{
	if (!IsHostileTarget(SourceAbilitySystem, TargetAbilitySystem)
		|| !Definition.EffectClass
		|| !FMath::IsFinite(Definition.Magnitude)
		|| !FMath::IsFinite(Definition.DurationSeconds)
		|| Definition.DurationSeconds <= 0.0f)
	{
		return FActiveGameplayEffectHandle();
	}

	const UPRStatusGameplayEffect* StatusEffect = Definition.EffectClass->GetDefaultObject<UPRStatusGameplayEffect>();
	const FGameplayTag StatusTag = StatusEffect ? StatusEffect->GetStatusTag() : FGameplayTag();
	if (!StatusTag.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	if (StatusTag == ProjectRiftGameplayTags::State_Stunned)
	{
		ClearHitStagger(TargetAbilitySystem);
	}

	FGameplayTagContainer ExistingStatusTags;
	ExistingStatusTags.AddTag(StatusTag);
	TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(ExistingStatusTags);

	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		Definition.EffectClass,
		1.0f,
		MakeCombatEffectContext(SourceAbilitySystem, TargetAbilitySystem, SourceObject));
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetDuration(Definition.DurationSeconds, true);
	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Status_Magnitude, Definition.Magnitude);
	SpecHandle.Data->AddDynamicAssetTag(StatusTag);
	if (StatusTag == ProjectRiftGameplayTags::Status_Debuff_Polluted)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Damage, Definition.Magnitude);
		SpecHandle.Data->AddDynamicAssetTag(ProjectRiftGameplayTags::Damage_Type_Pollution);
	}

	const FActiveGameplayEffectHandle ActiveHandle = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetAbilitySystem);
	if (ActiveHandle.IsValid() && StatusTag == ProjectRiftGameplayTags::State_Stunned)
	{
		TargetAbilitySystem->CancelAllAbilities();
	}
	return ActiveHandle;
}

void UPRCombatEffectLibrary::ClearNegativeStatusEffects(UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!TargetAbilitySystem)
	{
		return;
	}

	FGameplayTagContainer NegativeStatusTags;
	NegativeStatusTags.AddTag(ProjectRiftGameplayTags::Status_Debuff_Polluted);
	NegativeStatusTags.AddTag(ProjectRiftGameplayTags::Status_Debuff_Slowed);
	NegativeStatusTags.AddTag(ProjectRiftGameplayTags::State_Stunned);
	NegativeStatusTags.AddTag(ProjectRiftGameplayTags::State_HitStaggered);
	NegativeStatusTags.AddTag(ProjectRiftGameplayTags::Status_Debuff_Revealed);
	TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(NegativeStatusTags);
	if (UPRCombatFeedbackComponent* FeedbackComponent = ResolveFeedbackComponent(TargetAbilitySystem))
	{
		FeedbackComponent->ClearActiveStagger();
	}
}

bool UPRCombatEffectLibrary::ClearPurifiableStatusEffects(UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!TargetAbilitySystem)
	{
		return false;
	}
	FGameplayTagContainer PurifiableTags;
	PurifiableTags.AddTag(ProjectRiftGameplayTags::Status_Debuff_Polluted);
	PurifiableTags.AddTag(ProjectRiftGameplayTags::Status_Debuff_Slowed);
	PurifiableTags.AddTag(ProjectRiftGameplayTags::State_Stunned);
	const int32 RemovedCount = TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(PurifiableTags);
	return RemovedCount > 0;
}

FActiveGameplayEffectHandle UPRCombatEffectLibrary::ApplyReconRevealToTarget(
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem,
	const float DurationSeconds,
	UObject* SourceObject)
{
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		return FActiveGameplayEffectHandle();
	}
	return ApplyStatusEffectToTarget(
		SourceAbilitySystem,
		TargetAbilitySystem,
		FPRTargetStatusEffectDefinition(UPRReconRevealGameplayEffect::StaticClass(), 1.0f, DurationSeconds),
		SourceObject);
}

FString UPRCombatEffectLibrary::GetActiveNegativeStatusText(const UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!TargetAbilitySystem)
	{
		return TEXT("None");
	}

	TArray<FString> ActiveStatuses;
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned))
	{
		ActiveStatuses.Add(TEXT("STUNNED"));
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered))
	{
		ActiveStatuses.Add(TEXT("HIT-STAGGERED"));
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed))
	{
		ActiveStatuses.Add(TEXT("SLOWED"));
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted))
	{
		ActiveStatuses.Add(TEXT("POLLUTED"));
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Revealed))
	{
		ActiveStatuses.Add(TEXT("REVEALED"));
	}

	return ActiveStatuses.IsEmpty() ? TEXT("None") : FString::Join(ActiveStatuses, TEXT(" | "));
}
