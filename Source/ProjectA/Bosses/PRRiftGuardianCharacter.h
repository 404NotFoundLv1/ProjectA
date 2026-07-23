#pragma once

#include "Bosses/PRBossCharacter.h"
#include "PRRiftGuardianCharacter.generated.h"

/** Concrete first-production Boss.  Gameplay remains definition-driven. */
UCLASS()
class PROJECTA_API APRRiftGuardianCharacter : public APRBossCharacter
{
	GENERATED_BODY()

public:
	APRRiftGuardianCharacter();
};
