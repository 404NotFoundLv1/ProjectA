#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRHealthConsumableGameplayEffect.generated.h"

/**
 * Instant GameplayEffect used by the first-pass HealthInjector consumable.
 */
UCLASS()
class PROJECTA_API UPRHealthConsumableGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRHealthConsumableGameplayEffect();
};
