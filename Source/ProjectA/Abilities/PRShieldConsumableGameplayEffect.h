#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRShieldConsumableGameplayEffect.generated.h"

/**
 * Instant GameplayEffect used by the first-pass ShieldPack consumable.
 */
UCLASS()
class PROJECTA_API UPRShieldConsumableGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRShieldConsumableGameplayEffect();
};
