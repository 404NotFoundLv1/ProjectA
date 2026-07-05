#pragma once

#include "CoreMinimal.h"
#include "ProjectACharacter.h"
#include "PRCharacter.generated.h"

/**
 * Base playable avatar for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRCharacter : public AProjectACharacter
{
	GENERATED_BODY()

public:
	APRCharacter();
};
