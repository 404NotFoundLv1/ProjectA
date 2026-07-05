#include "Core/PRGameModeBase.h"

#include "Characters/PRCharacter.h"
#include "Core/PRGameState.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"

APRGameModeBase::APRGameModeBase()
{
	DefaultPawnClass = APRCharacter::StaticClass();
	PlayerControllerClass = APRPlayerController::StaticClass();
	PlayerStateClass = APRPlayerState::StaticClass();
	GameStateClass = APRGameState::StaticClass();
}
