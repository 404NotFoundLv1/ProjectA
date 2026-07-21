#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PREnemyTemporaryShieldGameplayEffect.generated.h"

/** Server-set shield amount used by enemy support actions; separate from player equipment shields. */
UCLASS()
class PROJECTA_API UPREnemyTemporaryShieldGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPREnemyTemporaryShieldGameplayEffect();
};
