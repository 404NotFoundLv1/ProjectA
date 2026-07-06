#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
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

	UFUNCTION(BlueprintCallable, Category = "ProjectRift|GAS|Input")
	bool AbilityInputTagPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "ProjectRift|GAS|Input")
	bool AbilityInputTagReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "ProjectRift|GAS|Input")
	bool TryActivateAbilityByInputTag(FGameplayTag InputTag);
};
