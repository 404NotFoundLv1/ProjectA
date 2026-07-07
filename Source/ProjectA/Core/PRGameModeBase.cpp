#include "Core/PRGameModeBase.h"

#include "Characters/PRCharacter.h"
#include "Core/PRGameState.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "UObject/ConstructorHelpers.h"

APRGameModeBase::APRGameModeBase()
{
	static ConstructorHelpers::FClassFinder<APRCharacter> ProjectRiftCharacterClass(TEXT("/Game/ProjectRift/Blueprints/Characters/BP_PRCharacter"));
	if (ProjectRiftCharacterClass.Succeeded())
	{
		DefaultPawnClass = ProjectRiftCharacterClass.Class;
	}
	else
	{
		DefaultPawnClass = APRCharacter::StaticClass();
	}
	PlayerControllerClass = APRPlayerController::StaticClass();
	PlayerStateClass = APRPlayerState::StaticClass();
	GameStateClass = APRGameState::StaticClass();
}
