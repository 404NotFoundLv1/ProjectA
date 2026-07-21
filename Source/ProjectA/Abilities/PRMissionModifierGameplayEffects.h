#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRMissionModifierGameplayEffects.generated.h"

class UTargetTagsGameplayEffectComponent;

UCLASS(Abstract)
class PROJECTA_API UPRMissionModifierGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRMissionModifierGameplayEffect();

protected:
	void AddRuleTag(const FGameplayTag& Tag, const FName& ComponentName);
};

UCLASS()
class PROJECTA_API UPRLowGravityMissionGameplayEffect : public UPRMissionModifierGameplayEffect
{
	GENERATED_BODY()
public:
	UPRLowGravityMissionGameplayEffect();
};

UCLASS()
class PROJECTA_API UPRShieldInterferenceMissionGameplayEffect : public UPRMissionModifierGameplayEffect
{
	GENERATED_BODY()
public:
	UPRShieldInterferenceMissionGameplayEffect();
};

UCLASS()
class PROJECTA_API UPRPollutionAmplifiedMissionGameplayEffect : public UPRMissionModifierGameplayEffect
{
	GENERATED_BODY()
public:
	UPRPollutionAmplifiedMissionGameplayEffect();
};

UCLASS()
class PROJECTA_API UPREliteReinforcementMissionGameplayEffect : public UPRMissionModifierGameplayEffect
{
	GENERATED_BODY()
public:
	UPREliteReinforcementMissionGameplayEffect();
};
