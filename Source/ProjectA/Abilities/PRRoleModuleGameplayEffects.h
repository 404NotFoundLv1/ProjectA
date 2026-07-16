#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PRRoleModuleGameplayEffects.generated.h"

/** Instant SetByCaller energy debit used by all data-driven role modules. */
UCLASS()
class PROJECTA_API UPRRoleModuleCostGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRRoleModuleCostGameplayEffect();
};

/** Infinite periodic SetByCaller energy regeneration for the active role. */
UCLASS()
class PROJECTA_API UPRRoleEnergyRegenGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRRoleEnergyRegenGameplayEffect();
};

UCLASS(Abstract)
class PROJECTA_API UPRRoleModuleCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRRoleModuleCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer);

protected:
	void AddCooldownTag(const FObjectInitializer& ObjectInitializer, const FGameplayTag& CooldownTag, const FName& ComponentName);
};

UCLASS()
class PROJECTA_API UPRAssaultChargeCooldownGameplayEffect : public UPRRoleModuleCooldownGameplayEffect
{
	GENERATED_BODY()

public:
	UPRAssaultChargeCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class PROJECTA_API UPRAssaultBlastCooldownGameplayEffect : public UPRRoleModuleCooldownGameplayEffect
{
	GENERATED_BODY()

public:
	UPRAssaultBlastCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class PROJECTA_API UPRAssaultShieldCooldownGameplayEffect : public UPRRoleModuleCooldownGameplayEffect
{
	GENERATED_BODY()

public:
	UPRAssaultShieldCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class PROJECTA_API UPRShieldRepairGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRShieldRepairGameplayEffect();
};

UCLASS()
class PROJECTA_API UPRShieldGeneratorAuraGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRShieldGeneratorAuraGameplayEffect();
};
