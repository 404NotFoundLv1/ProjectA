#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRTemporaryShieldGameplayEffect.generated.h"

/**
 * First-pass tactical shield effect used by the assault module.
 */
UCLASS()
class PROJECTA_API UPRTemporaryShieldGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRTemporaryShieldGameplayEffect();
};
