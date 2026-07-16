#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_EngineerDeployableAbility.h"
#include "GA_EngineerShieldGenerator.generated.h"

UCLASS()
class PROJECTA_API UGA_EngineerShieldGenerator : public UGA_EngineerDeployableAbility
{
	GENERATED_BODY()

protected:
	virtual EPRDeployableKind GetDeployableKind() const override { return EPRDeployableKind::ShieldGenerator; }
	virtual FGameplayTag GetModuleCooldownTag() const override;
	virtual TSubclassOf<UGameplayEffect> GetModuleCooldownEffectClass() const override;
};
