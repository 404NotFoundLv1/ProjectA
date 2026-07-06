#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "PRAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Base replicated player attributes for Project Rift.
 */
UCLASS()
class PROJECTA_API UPRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPRAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Attributes|Vital")
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "Attributes|Vital")
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MaxShield)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Attributes|Resource")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Energy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Attributes|Resource")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MaxEnergy)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Combat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, AttackPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category = "Attributes|Movement")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MoveSpeed)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CooldownReduction, Category = "Attributes|Combat")
	FGameplayAttributeData CooldownReduction;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, CooldownReduction)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingPower, Category = "Attributes|Support")
	FGameplayAttributeData HealingPower;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, HealingPower)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PollutionResistance, Category = "Attributes|Resistance")
	FGameplayAttributeData PollutionResistance;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, PollutionResistance)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldEnergy);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy);

	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

	UFUNCTION()
	void OnRep_CooldownReduction(const FGameplayAttributeData& OldCooldownReduction);

	UFUNCTION()
	void OnRep_HealingPower(const FGameplayAttributeData& OldHealingPower);

	UFUNCTION()
	void OnRep_PollutionResistance(const FGameplayAttributeData& OldPollutionResistance);
};
