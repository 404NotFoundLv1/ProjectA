#pragma once

#include "CoreMinimal.h"
#include "ProjectAPlayerController.h"
#include "PRPlayerController.generated.h"

/**
 * Player-owned input and UI entry point for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRPlayerController : public AProjectAPlayerController
{
	GENERATED_BODY()

public:
	APRPlayerController();
};
