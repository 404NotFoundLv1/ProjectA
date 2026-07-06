#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "PRAbilitySystemComponent.generated.h"

/**
 * Project Rift ability system component.
 */
UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPRAbilitySystemComponent();
};
