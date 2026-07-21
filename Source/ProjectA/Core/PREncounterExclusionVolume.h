#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "PREncounterExclusionVolume.generated.h"

/** Server-only authoring volume: encounter directors never select a point inside it. */
UCLASS()
class PROJECTA_API APREncounterExclusionVolume : public AVolume
{
	GENERATED_BODY()
public:
	APREncounterExclusionVolume();
	bool ExcludesLocation(const FVector& Location) const;
};
