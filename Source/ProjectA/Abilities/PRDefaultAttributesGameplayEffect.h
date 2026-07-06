#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRDefaultAttributesGameplayEffect.generated.h"

/**
 * Instant GameplayEffect that initializes player base attributes.
 */
UCLASS()
class PROJECTA_API UPRDefaultAttributesGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRDefaultAttributesGameplayEffect();
};
