#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Items/PRItemTypes.h"
#include "PRAffixGameplayEffects.generated.h"

/** Explicit, traceable base class for persistent equipment-affix modifiers. */
UCLASS(Abstract)
class PROJECTA_API UPRAffixModifierGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRAffixModifierGameplayEffect();

	EPRAffixAttribute GetAffixAttribute() const { return AffixAttribute; }
	EPRAffixModifierType GetAffixModifierType() const { return AffixModifierType; }

protected:
	void Configure(EPRAffixAttribute InAttribute, EPRAffixModifierType InModifierType);

	UPROPERTY(VisibleDefaultsOnly, Category = "Affix")
	EPRAffixAttribute AffixAttribute = EPRAffixAttribute::MaxHealth;

	UPROPERTY(VisibleDefaultsOnly, Category = "Affix")
	EPRAffixModifierType AffixModifierType = EPRAffixModifierType::Additive;
};

#define PROJECTRIFT_AFFIX_EFFECT_DECLARATION(ClassName) \
	UCLASS() \
	class PROJECTA_API ClassName : public UPRAffixModifierGameplayEffect \
	{ \
		GENERATED_BODY() \
	public: \
		ClassName(); \
	};

// UHT intentionally requires each reflected subclass to be written explicitly below.
UCLASS()
class PROJECTA_API UPRAffixMaxHealthAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxHealthAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMaxHealthPercentageGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxHealthPercentageGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMaxShieldAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxShieldAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMaxShieldPercentageGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxShieldPercentageGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMaxEnergyAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxEnergyAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMaxEnergyPercentageGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMaxEnergyPercentageGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixAttackPowerAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixAttackPowerAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixAttackPowerPercentageGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixAttackPowerPercentageGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMoveSpeedAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMoveSpeedAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixMoveSpeedPercentageGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixMoveSpeedPercentageGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixHealingPowerAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixHealingPowerAdditiveGameplayEffect(); };
UCLASS()
class PROJECTA_API UPRAffixPollutionResistanceAdditiveGameplayEffect : public UPRAffixModifierGameplayEffect { GENERATED_BODY() public: UPRAffixPollutionResistanceAdditiveGameplayEffect(); };

#undef PROJECTRIFT_AFFIX_EFFECT_DECLARATION
