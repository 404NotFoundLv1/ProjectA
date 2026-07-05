#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PRGameState.generated.h"

/**
 * Replicated global state shared by lobby and rift flows.
 */
UCLASS()
class PROJECTA_API APRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APRGameState();
};
