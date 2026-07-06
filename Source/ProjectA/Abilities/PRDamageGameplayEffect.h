#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRDamageGameplayEffect.generated.h"

/**
 * Shared damage entry point. Callers provide Data.Damage through SetByCaller.
 */
UCLASS()
class PROJECTA_API UPRDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRDamageGameplayEffect();
};
