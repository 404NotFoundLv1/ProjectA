#pragma once

#include "CoreMinimal.h"
#include "ProjectAGameMode.h"
#include "PRGameModeBase.generated.h"

/**
 * Server-authoritative base rule set for ProjectRift game modes.
 */
UCLASS()
class PROJECTA_API APRGameModeBase : public AProjectAGameMode
{
	GENERATED_BODY()

public:
	APRGameModeBase();
};
