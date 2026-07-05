#include "Player/PRPlayerController.h"

#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FString GetLobbyReadyLine(const APlayerState* PlayerState)
{
	const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
	const bool bReady = ProjectRiftPlayerState && ProjectRiftPlayerState->IsReady();
	FString DisplayName = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetPlayerDisplayName() : FString();

	if (DisplayName.IsEmpty() && PlayerState)
	{
		DisplayName = PlayerState->GetPlayerName();
	}

	if (DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Player");
	}

	const int32 PlayerId = PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
	return FString::Printf(
		TEXT("P%d %s: %s"),
		PlayerId,
		*DisplayName,
		bReady ? TEXT("READY") : TEXT("WAITING"));
}
}

APRPlayerController::APRPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextAsset(TEXT("/Game/ProjectRift/Input/IMC_Default.IMC_Default"));
	if (DefaultContextAsset.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContextAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContextAsset(TEXT("/Game/ProjectRift/Input/IMC_MouseLook.IMC_MouseLook"));
	if (MouseLookContextAsset.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookContextAsset.Object);
	}
}

void APRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		GetWorldTimerManager().SetTimer(
			LobbyReadyDebugTimerHandle,
			this,
			&APRPlayerController::RefreshLobbyReadyDebugDisplay,
			1.0f,
			true,
			0.25f);
	}
}

void APRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LobbyReadyDebugTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void APRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APRPlayerController::ToggleReady);
	}
}

void APRPlayerController::ToggleReady()
{
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const bool bNewReadyState = ProjectRiftPlayerState ? !ProjectRiftPlayerState->IsReady() : true;

	ServerSetReady(bNewReadyState);
}

void APRPlayerController::ServerSetReady_Implementation(const bool bReady)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	if (!ProjectRiftPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ServerSetReady failed because APRPlayerState is missing."));
		return;
	}

	ProjectRiftPlayerState->SetReady(bReady);
}

void APRPlayerController::RefreshLobbyReadyDebugDisplay()
{
	if (!IsLocalPlayerController() || !GEngine)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	FString ReadyList = TEXT("Lobby Ready\n");
	for (const APlayerState* ListedPlayerState : GameState->PlayerArray)
	{
		ReadyList += GetLobbyReadyLine(ListedPlayerState);
		ReadyList += LINE_TERMINATOR;
	}

	const int32 MessageKey = IsLocalController() ? 16016 : 16017;
	GEngine->AddOnScreenDebugMessage(MessageKey, 1.1f, FColor::Green, ReadyList);
}
