#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PRGameplayAbility.generated.h"

/**
 * Base GameplayAbility for Project Rift abilities.
 */
UCLASS(Abstract)
class PROJECTA_API UPRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPRGameplayAbility();
};
