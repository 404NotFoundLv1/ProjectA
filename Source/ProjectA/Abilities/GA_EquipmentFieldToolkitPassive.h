#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRGameplayAbility.h"
#include "GA_EquipmentFieldToolkitPassive.generated.h"

/** Passive marker ability granted by the v0.7.1 Field Toolkit test equipment. */
UCLASS()
class PROJECTA_API UGA_EquipmentFieldToolkitPassive : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EquipmentFieldToolkitPassive();
};

