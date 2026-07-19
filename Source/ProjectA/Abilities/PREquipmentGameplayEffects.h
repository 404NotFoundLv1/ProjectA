#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "PREquipmentGameplayEffects.generated.h"

/** Placeholder armor grant used by the v0.7.1 equipment test kit. */
UCLASS()
class PROJECTA_API UPRTestArmorEquipmentGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRTestArmorEquipmentGameplayEffect();
};

/** Placeholder chip grant used by the v0.7.1 equipment test kit. */
UCLASS()
class PROJECTA_API UPRTestChipEquipmentGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRTestChipEquipmentGameplayEffect();
};

/** Placeholder tool grant used by the v0.7.1 equipment test kit. */
UCLASS()
class PROJECTA_API UPRTestToolEquipmentGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRTestToolEquipmentGameplayEffect();
};

