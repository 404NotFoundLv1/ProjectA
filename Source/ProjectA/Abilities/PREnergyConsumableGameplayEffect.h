#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PREnergyConsumableGameplayEffect.generated.h"

/** Instant +30 Energy effect for the data-driven EnergyCell item. */
UCLASS()
class PROJECTA_API UPREnergyConsumableGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPREnergyConsumableGameplayEffect();
};
