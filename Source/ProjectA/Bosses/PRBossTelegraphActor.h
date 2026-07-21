#pragma once

#include "Bosses/PRBossTypes.h"
#include "GameFramework/Actor.h"
#include "PRBossTelegraphActor.generated.h"

class UPointLightComponent;
class UTextRenderComponent;

/** Replicated, intentionally lightweight telegraph fallback for Boss patterns. */
UCLASS()
class PROJECTA_API APRBossTelegraphActor : public AActor
{
	GENERATED_BODY()

public:
	APRBossTelegraphActor();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Telegraph")
	void InitializeTelegraph(const FPRBossTelegraphDefinition& InDefinition, FName PatternId);

private:
	UPROPERTY(VisibleAnywhere, Category = "Boss|Telegraph") TObjectPtr<UPointLightComponent> WarningLight;
	UPROPERTY(VisibleAnywhere, Category = "Boss|Telegraph") TObjectPtr<UTextRenderComponent> WarningText;
};
