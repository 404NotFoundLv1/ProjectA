#include "Combat/PRCombatHitCameraShake.h"

#include "Shakes/PerlinNoiseCameraShakePattern.h"


UPRCombatHitCameraShake::UPRCombatHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("HitPerlinPattern"));
	Pattern->Duration = 0.14f;
	Pattern->BlendInTime = 0.015f;
	Pattern->BlendOutTime = 0.08f;
	Pattern->LocationAmplitudeMultiplier = 0.35f;
	Pattern->LocationFrequencyMultiplier = 18.0f;
	Pattern->RotationAmplitudeMultiplier = 0.8f;
	Pattern->RotationFrequencyMultiplier = 22.0f;
	Pattern->X.Amplitude = 0.20f;
	Pattern->Y.Amplitude = 0.25f;
	Pattern->Z.Amplitude = 0.16f;
	Pattern->Pitch.Amplitude = 0.55f;
	Pattern->Yaw.Amplitude = 0.70f;
	Pattern->Roll.Amplitude = 0.30f;
	SetRootShakePattern(Pattern);
}
