#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRPrimaryAttackDamageGameplayEffect.generated.h"

/**
 * Temporary primary-attack damage effect until v0.2.6 introduces the shared damage pipeline.
 */
UCLASS()
class PROJECTA_API UPRPrimaryAttackDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRPrimaryAttackDamageGameplayEffect();
};
