#include "Player/PRPlayerController.h"

#include "Core/PRShipLobbyGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "UI/PRGASDebugWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName AssaultRoleModuleName(TEXT("Ability.Role.Assault"));

FString GetLobbyReadyLine(const APlayerState* PlayerState)
{
	const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
	const bool bReady = ProjectRiftPlayerState && ProjectRiftPlayerState->IsReady();
	const FName SelectedRoleModule = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetSelectedRoleModule() : NAME_None;
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
		TEXT("P%d %s: %s | Role: %s"),
		PlayerId,
		*DisplayName,
		bReady ? TEXT("READY") : TEXT("WAITING"),
		SelectedRoleModule.IsNone() ? TEXT("None") : *SelectedRoleModule.ToString());
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

	static ConstructorHelpers::FClassFinder<UPRGASDebugWidget> GASDebugWidgetAsset(TEXT("/Game/ProjectRift/UI/WBP_GASDebugHUD"));
	if (GASDebugWidgetAsset.Succeeded())
	{
		GASDebugWidgetClass = GASDebugWidgetAsset.Class;
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

		CreateGASDebugHUD();
	}
}

void APRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyGASDebugHUD();
	GetWorldTimerManager().ClearTimer(LobbyReadyDebugTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void APRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &APRPlayerController::SelectAssaultRoleModule);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APRPlayerController::ToggleReady);
		InputComponent->BindKey(EKeys::O, IE_Pressed, this, &APRPlayerController::StartRiftMission);
	}
}

void APRPlayerController::SelectAssaultRoleModule()
{
	SelectRoleModule(AssaultRoleModuleName);
}

void APRPlayerController::SelectRoleModule(const FName RoleModule)
{
	ServerSetSelectedRoleModule(RoleModule);
}

void APRPlayerController::ToggleReady()
{
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const bool bNewReadyState = ProjectRiftPlayerState ? !ProjectRiftPlayerState->IsReady() : true;

	ServerSetReady(bNewReadyState);
}

void APRPlayerController::StartRiftMission()
{
	ServerStartRiftMission();
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

void APRPlayerController::ServerSetSelectedRoleModule_Implementation(const FName RoleModule)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	if (!ProjectRiftPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ServerSetSelectedRoleModule failed because APRPlayerState is missing."));
		return;
	}

	if (RoleModule.IsNone())
	{
		ProjectRiftPlayerState->SetSelectedRoleModule(NAME_None);
		return;
	}

	if (RoleModule == AssaultRoleModuleName || RoleModule == FName(TEXT("Role.Assault")))
	{
		ProjectRiftPlayerState->SetSelectedRoleModule(AssaultRoleModuleName);
		return;
	}

	UE_LOG(LogProjectA, Warning, TEXT("Unsupported role module requested: %s"), *RoleModule.ToString());
}

void APRPlayerController::ServerStartRiftMission_Implementation()
{
	UWorld* World = GetWorld();
	APRShipLobbyGameMode* LobbyGameMode = World ? Cast<APRShipLobbyGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!LobbyGameMode)
	{
		ClientMessage(TEXT("Rift mission can only be started from the ship lobby."));
		return;
	}

	LobbyGameMode->StartRiftMission(this);
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
	ReadyList += TEXT("1: Assault | P: Ready | Host O: Start Rift\n");
	for (const APlayerState* ListedPlayerState : GameState->PlayerArray)
	{
		ReadyList += GetLobbyReadyLine(ListedPlayerState);
		ReadyList += LINE_TERMINATOR;
	}

	const int32 MessageKey = IsLocalController() ? 16016 : 16017;
	GEngine->AddOnScreenDebugMessage(MessageKey, 1.1f, FColor::Green, ReadyList);
}

void APRPlayerController::CreateGASDebugHUD()
{
	if (!IsLocalPlayerController() || GASDebugWidget)
	{
		return;
	}

	if (!GASDebugWidgetClass)
	{
		GASDebugWidgetClass = UPRGASDebugWidget::StaticClass();
	}

	GASDebugWidget = CreateWidget<UPRGASDebugWidget>(this, GASDebugWidgetClass);
	if (GASDebugWidget)
	{
		GASDebugWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		GASDebugWidget->AddToPlayerScreen(20);
		GASDebugWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
		GASDebugWidget->SetPositionInViewport(FVector2D(16.0, 16.0), false);
		GASDebugWidget->SetDesiredSizeInViewport(FVector2D(420.0, 220.0));
	}
}

void APRPlayerController::DestroyGASDebugHUD()
{
	if (GASDebugWidget)
	{
		GASDebugWidget->RemoveFromParent();
		GASDebugWidget = nullptr;
	}
}
