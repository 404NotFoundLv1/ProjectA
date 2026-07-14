#include "Abilities/PRCombatEffectLibrary.h"

#include "Abilities/PRDamageGameplayEffect.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatUnitInterface.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameplayEffect.h"
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
}

bool UPRCombatEffectLibrary::IsHostileTarget(
	const UAbilitySystemComponent* SourceAbilitySystem,
	const UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!SourceAbilitySystem || !TargetAbilitySystem || SourceAbilitySystem == TargetAbilitySystem || HasInactiveState(TargetAbilitySystem))
	{
		return false;
	}

	const EPRCombatantKind SourceKind = GetCombatantKind(SourceAbilitySystem);
	const EPRCombatantKind TargetKind = GetCombatantKind(TargetAbilitySystem);
	return (SourceKind == EPRCombatantKind::Player && TargetKind == EPRCombatantKind::Enemy)
		|| (SourceKind == EPRCombatantKind::Enemy && TargetKind == EPRCombatantKind::Player);
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
	TargetAbilitySystem->RemoveActiveEffectsWithGrantedTags(NegativeStatusTags);
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
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed))
	{
		ActiveStatuses.Add(TEXT("SLOWED"));
	}
	if (TargetAbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted))
	{
		ActiveStatuses.Add(TEXT("POLLUTED"));
	}

	return ActiveStatuses.IsEmpty() ? TEXT("None") : FString::Join(ActiveStatuses, TEXT(" | "));
}
