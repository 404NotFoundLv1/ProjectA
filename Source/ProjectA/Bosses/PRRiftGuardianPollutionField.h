#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRRiftGuardianPollutionField.generated.h"

class APRBossCharacter;
class USphereComponent;

/** Replicated server-owned pollution zone. Damage remains in the shared GAS status pipeline. */
UCLASS()
class PROJECTA_API APRRiftGuardianPollutionField : public AActor
{
	GENERATED_BODY()

public:
	APRRiftGuardianPollutionField();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeField(APRBossCharacter* InSourceBoss, float InRadius, float InDurationSeconds, float InMagnitude);

private:
	void PulsePollution();

	UPROPERTY(VisibleAnywhere, Category = "Boss|Pollution") TObjectPtr<USphereComponent> RadiusComponent;
	TWeakObjectPtr<APRBossCharacter> SourceBoss;
	float Radius = 0.0f;
	float Magnitude = 0.0f;
	FTimerHandle PulseTimer;
};
