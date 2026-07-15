#pragma once

#include "Camera/CameraShakeBase.h"
#include "PRCombatHitCameraShake.generated.h"

/** Brief, intentionally light native camera shake for local received-hit feedback. */
UCLASS()
class PROJECTA_API UPRCombatHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UPRCombatHitCameraShake(const FObjectInitializer& ObjectInitializer);
};
