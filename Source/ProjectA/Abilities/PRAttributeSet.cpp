#include "Abilities/PRAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Combat/PRCombatUnitInterface.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

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
		const float DamageAbsorbedByShield = FMath::Min(OldShield, LocalDamageDone);
		const float DamageToHealth = LocalDamageDone - DamageAbsorbedByShield;

		SetShield(FMath::Clamp(OldShield - DamageAbsorbedByShield, 0.0f, GetMaxShield()));
		if (DamageToHealth > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - DamageToHealth, 0.0f, GetMaxHealth()));
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
