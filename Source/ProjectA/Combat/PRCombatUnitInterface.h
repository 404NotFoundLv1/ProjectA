#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Interface.h"
#include "PRCombatUnitInterface.generated.h"

UINTERFACE()
class PROJECTA_API UPRCombatUnitInterface : public UInterface
{
	GENERATED_BODY()
};

/** Common lifecycle hook used by the shared AttributeSet for players and enemies. */
class PROJECTA_API IPRCombatUnitInterface
{
	GENERATED_BODY()

public:
	virtual bool IsCombatUnitInactive() const = 0;
	virtual void HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext) = 0;
	/** Called only after the shared Damage meta attribute has resolved shield and health loss. */
	virtual void HandleCombatUnitDamageResolved(AActor* DamageSource, float ResolvedDamage) {}
};
