#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PRPlayerState.generated.h"

/**
 * Player-owned replicated data that should survive pawn replacement.
 */
UCLASS()
class PROJECTA_API APRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APRPlayerState();
};
