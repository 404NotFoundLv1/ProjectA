#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRHitStaggerGameplayEffect.generated.h"

/** Duration effect used by the combat feedback pipeline to carry hit stagger state. */
UCLASS()
class PROJECTA_API UPRHitStaggerGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRHitStaggerGameplayEffect(const FObjectInitializer& ObjectInitializer);
};
