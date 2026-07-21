#pragma once

#include "Bosses/PRBossTypes.h"
#include "Components/ActorComponent.h"
#include "PRBossWeakPointComponent.generated.h"

/** Logical weak-point definition paired with the Boss character's native collision sphere. */
UCLASS(ClassGroup=(Boss), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTA_API UPRBossWeakPointComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRBossWeakPointComponent();
	void Configure(const FPRBossWeakPointDefinition& InDefinition);
	UFUNCTION(BlueprintPure, Category = "Boss|WeakPoint") FName GetWeakPointId() const { return WeakPointId; }
	UFUNCTION(BlueprintPure, Category = "Boss|WeakPoint") float GetDamageMultiplier() const { return DamageMultiplier; }

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|WeakPoint", meta = (AllowPrivateAccess = "true")) FName WeakPointId;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss|WeakPoint", meta = (AllowPrivateAccess = "true")) float DamageMultiplier = 1.0f;
};
