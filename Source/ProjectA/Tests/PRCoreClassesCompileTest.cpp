#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/PRGameInstance.h"
#include "Core/PRGameModeBase.h"
#include "Core/PRGameState.h"
#include "Characters/PRCharacter.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"

#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRCoreClassesCompileTest, "ProjectRift.Core.ClassesCompile", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRCoreClassesCompileTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("APRGameModeBase derives from AGameModeBase"), APRGameModeBase::StaticClass()->IsChildOf(AGameModeBase::StaticClass()));
	TestTrue(TEXT("APRGameState derives from AGameStateBase"), APRGameState::StaticClass()->IsChildOf(AGameStateBase::StaticClass()));
	TestTrue(TEXT("APRPlayerState derives from APlayerState"), APRPlayerState::StaticClass()->IsChildOf(APlayerState::StaticClass()));
	TestTrue(TEXT("APRPlayerController derives from APlayerController"), APRPlayerController::StaticClass()->IsChildOf(APlayerController::StaticClass()));
	TestTrue(TEXT("APRCharacter derives from ACharacter"), APRCharacter::StaticClass()->IsChildOf(ACharacter::StaticClass()));
	TestTrue(TEXT("UPRGameInstance derives from UGameInstance"), UPRGameInstance::StaticClass()->IsChildOf(UGameInstance::StaticClass()));

	return true;
}

#endif
