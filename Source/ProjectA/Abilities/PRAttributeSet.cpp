#include "Abilities/PRAttributeSet.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/PRCombatUnitInterface.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
bool DecodeFeedbackPolicy(const float EncodedValue, EPRCombatFeedbackPolicy& OutPolicy)
{
	if (!FMath::IsFinite(EncodedValue))
	{
		return false;
	}

	if (EncodedValue == static_cast<float>(EPRCombatFeedbackPolicy::None)
		|| EncodedValue == static_cast<float>(EPRCombatFeedbackPolicy::TargetOnly)
		|| EncodedValue == static_cast<float>(EPRCombatFeedbackPolicy::TargetAndSource))
	{
		OutPolicy = static_cast<EPRCombatFeedbackPolicy>(static_cast<uint8>(EncodedValue));
		return true;
	}
	return false;
}

bool DecodeHitReactionStrength(const float EncodedValue, EPRHitReactionStrength& OutStrength)
{
	if (!FMath::IsFinite(EncodedValue))
	{
		return false;
	}

	if (EncodedValue == static_cast<float>(EPRHitReactionStrength::None)
		|| EncodedValue == static_cast<float>(EPRHitReactionStrength::Light)
		|| EncodedValue == static_cast<float>(EPRHitReactionStrength::Heavy))
	{
		OutStrength = static_cast<EPRHitReactionStrength>(static_cast<uint8>(EncodedValue));
		return true;
	}
	return false;
}

bool DecodeStructuredDamageRequest(
	const FGameplayEffectSpec& EffectSpec,
	FPRDamageRequest& OutDamageRequest)
{
	FGameplayTagContainer AssetTags;
	EffectSpec.GetAllAssetTags(AssetTags);
	if (!AssetTags.HasTagExact(ProjectRiftGameplayTags::Data_CombatFeedback_Request))
	{
		return false;
	}

	OutDamageRequest.BaseDamage = EffectSpec.GetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_Damage,
		false,
		0.0f);
	OutDamageRequest.DamageType = AssetTags.HasTagExact(ProjectRiftGameplayTags::Damage_Type_Pollution)
		? ProjectRiftGameplayTags::Damage_Type_Pollution
		: ProjectRiftGameplayTags::Damage_Type_Physical;

	const float FeedbackPolicyValue = EffectSpec.GetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_CombatFeedback_Policy,
		false,
		0.0f);
	const float HitReactionStrengthValue = EffectSpec.GetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_HitReaction_Strength,
		false,
		0.0f);
	const bool bFeedbackPolicyValid = DecodeFeedbackPolicy(
		FeedbackPolicyValue,
		OutDamageRequest.FeedbackPolicy);
	const bool bHitReactionStrengthValid = DecodeHitReactionStrength(
		HitReactionStrengthValue,
		OutDamageRequest.HitReaction.Strength);
	OutDamageRequest.HitReaction.DurationSeconds = EffectSpec.GetSetByCallerMagnitude(
		ProjectRiftGameplayTags::Data_HitReaction_Duration,
		false,
		0.0f);
	if (!bFeedbackPolicyValid || !bHitReactionStrengthValid)
	{
		OutDamageRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::None;
		OutDamageRequest.HitReaction.Strength = EPRHitReactionStrength::None;
		OutDamageRequest.HitReaction.DurationSeconds = 0.0f;
	}

	if (const FHitResult* HitResult = EffectSpec.GetEffectContext().GetHitResult())
	{
		OutDamageRequest.HitResult = *HitResult;
	}
	return true;
}
}

UPRAttributeSet::UPRAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitShield(50.0f);
	InitMaxShield(50.0f);
	InitEnergy(100.0f);
	InitMaxEnergy(100.0f);
	InitAttackPower(10.0f);
	InitMoveSpeed(600.0f);
	InitCooldownReduction(0.0f);
	InitHealingPower(0.0f);
	InitPollutionResistance(0.0f);
	InitDamage(0.0f);
}

void UPRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, CooldownReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, HealingPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPRAttributeSet, PollutionResistance, COND_None, REPNOTIFY_Always);
}

void UPRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
	}
}

void UPRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamageDone = FMath::Max(GetDamage(), 0.0f);
		SetDamage(0.0f);

		if (LocalDamageDone <= 0.0f)
		{
			return;
		}

		const float OldShield = GetShield();
		const float OldHealth = GetHealth();
		const float DamageAbsorbedByShield = FMath::Min(OldShield, LocalDamageDone);
		const float DamageToHealth = LocalDamageDone - DamageAbsorbedByShield;

		SetShield(FMath::Clamp(OldShield - DamageAbsorbedByShield, 0.0f, GetMaxShield()));
		if (DamageToHealth > 0.0f)
		{
			SetHealth(FMath::Clamp(OldHealth - DamageToHealth, 0.0f, GetMaxHealth()));
		}

		FPRDamageRequest DamageRequest;
		if (DecodeStructuredDamageRequest(Data.EffectSpec, DamageRequest))
		{
			FPRResolvedDamage ResolvedDamage;
			ResolvedDamage.HitReactionStrength = DamageRequest.HitReaction.Strength;
			ResolvedDamage.ShieldDamage = DamageAbsorbedByShield;
			ResolvedDamage.HealthDamage = FMath::Min(DamageToHealth, OldHealth);
			ResolvedDamage.bShieldBroken = OldShield > 0.0f && GetShield() <= 0.0f;
			ResolvedDamage.bLethal = OldHealth > 0.0f && GetHealth() <= 0.0f;
			UPRCombatEffectLibrary::DispatchResolvedDamageFeedbackInternal(
				Data.EffectSpec.GetEffectContext().GetInstigatorAbilitySystemComponent(),
				&Data.Target,
				DamageRequest,
				ResolvedDamage,
				Data.EffectSpec.GetEffectContext(),
				true);
		}

		if (GetHealth() <= 0.0f)
		{
			if (IPRCombatUnitInterface* CombatUnit = Cast<IPRCombatUnitInterface>(Data.Target.GetAvatarActor()))
			{
				CombatUnit->HandleCombatUnitHealthDepleted(Data.EffectSpec.GetEffectContext());
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, GetMaxShield()));
	}
}

void UPRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, Health, OldHealth);
}

void UPRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, MaxHealth, OldMaxHealth);
}

void UPRAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, Shield, OldShield);
}

void UPRAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, MaxShield, OldMaxShield);
}

void UPRAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldEnergy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, Energy, OldEnergy);
}

void UPRAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, MaxEnergy, OldMaxEnergy);
}

void UPRAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, AttackPower, OldAttackPower);
}

void UPRAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, MoveSpeed, OldMoveSpeed);
}

void UPRAttributeSet::OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, CooldownReduction, OldCooldownReduction);
}

void UPRAttributeSet::OnRep_HealingPower(const FGameplayAttributeData& OldHealingPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, HealingPower, OldHealingPower);
}

void UPRAttributeSet::OnRep_PollutionResistance(const FGameplayAttributeData& OldPollutionResistance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPRAttributeSet, PollutionResistance, OldPollutionResistance);
}
