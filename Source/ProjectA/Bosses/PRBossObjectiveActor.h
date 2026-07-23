#pragma once

#include "Core/PRObjectiveTypeActors.h"
#include "PRBossObjectiveActor.generated.h"

class APRBossEncounterController;
class APRBossCharacter;

/** A Hunt-semantic objective that owns an authored Boss encounter boundary. */
UCLASS()
class PROJECTA_API APRBossObjectiveActor : public APRHuntObjectiveActor
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

protected:
	virtual void HandleObjectiveActivated(AController* ActivatingController) override;

	UFUNCTION()
	void HandleBossDefeated(APRBossCharacter* DefeatedBoss);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss|Objective")
	TObjectPtr<APRBossEncounterController> BossEncounterController;
};
