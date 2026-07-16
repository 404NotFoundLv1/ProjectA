#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_EngineerDeployableAbility.h"
#include "GA_EngineerRepairDrone.generated.h"

UCLASS()
class PROJECTA_API UGA_EngineerRepairDrone : public UGA_EngineerDeployableAbility
{
	GENERATED_BODY()

protected:
	virtual EPRDeployableKind GetDeployableKind() const override { return EPRDeployableKind::RepairDrone; }
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;
};
