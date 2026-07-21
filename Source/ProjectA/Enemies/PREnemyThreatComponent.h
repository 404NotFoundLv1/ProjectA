#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PREnemyThreatComponent.generated.h"

class APRCharacter;
enum class EPREnemyTargetPolicy : uint8;

UCLASS(ClassGroup=(Enemy), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTA_API UPREnemyThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPREnemyThreatComponent();
	void AddThreat(APRCharacter* Source, float Amount);
	APRCharacter* SelectTarget(EPREnemyTargetPolicy Policy) const;
	float GetThreatFor(const APRCharacter* Source) const;
	void PruneInvalidThreat();

private:
	TMap<TWeakObjectPtr<APRCharacter>, float> ThreatByPlayer;
};
