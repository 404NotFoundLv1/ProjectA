#pragma once

#include "CoreMinimal.h"
#include "Bosses/PRBossTypes.h"
#include "GameFramework/Actor.h"
#include "PRRiftGuardianChargeAction.generated.h"

class APRBossCharacter;

/** Server-driven swept movement for the Guardian's locked charge.  The Boss itself remains the replicated visual actor. */
UCLASS()
class PROJECTA_API APRRiftGuardianChargeAction : public AActor
{
	GENERATED_BODY()

public:
	APRRiftGuardianChargeAction();
	virtual void Tick(float DeltaSeconds) override;

	void InitializeCharge(APRBossCharacter* InSourceBoss, const FVector& InLockedTargetLocation, const FPRBossAbilityPatternDefinition& Pattern);

private:
	void ApplyChargeDamage(const TArray<FHitResult>& Hits);

	TWeakObjectPtr<APRBossCharacter> SourceBoss;
	FVector StartLocation = FVector::ZeroVector;
	FVector ChargeDirection = FVector::ForwardVector;
	float ChargeDistance = 0.0f;
	float TravelledDistance = 0.0f;
	float ChargeSpeed = 1400.0f;
	float DamageRadius = 180.0f;
	float BaseDamage = 0.0f;
	FGameplayTag DamageType;
	FPRHitReactionDefinition HitReaction;
	TSet<TObjectKey<AActor>> DamagedActors;
};
