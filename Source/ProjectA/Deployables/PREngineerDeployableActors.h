#pragma once

#include "CoreMinimal.h"
#include "Deployables/PRDeployableActor.h"
#include "PREngineerDeployableActors.generated.h"

UCLASS()
class PROJECTA_API APRSentryDeployable : public APRDeployableActor
{
	GENERATED_BODY()
public:
	virtual void ConfigureFromModule(const UPREngineerModuleDataAsset* Module) override;
private:
	void FireAtNearestEnemy();
	float EffectRange = 1600.0f;
	float DamageAmount = 5.0f;
	FTimerHandle EffectTimer;
};

UCLASS()
class PROJECTA_API APRRepairDroneDeployable : public APRDeployableActor
{
	GENERATED_BODY()
public:
	virtual void ConfigureFromModule(const UPREngineerModuleDataAsset* Module) override;
private:
	void RepairMostDamagedFriendly();
	float EffectRange = 900.0f;
	float RepairAmount = 6.0f;
	FTimerHandle EffectTimer;
};

UCLASS()
class PROJECTA_API APRShieldGeneratorDeployable : public APRDeployableActor
{
	GENERATED_BODY()
public:
	virtual void ConfigureFromModule(const UPREngineerModuleDataAsset* Module) override;
private:
	void RefreshFriendlyAura();
	float EffectRange = 700.0f;
	FTimerHandle EffectTimer;
};
