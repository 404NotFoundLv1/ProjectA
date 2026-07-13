#include "Core/PRGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Persistence/PRSaveSubsystem.h"
#include "ProjectA.h"
#include "UI/PRDebugUILayout.h"
#include "UI/PRProfileDebugWidget.h"
#include "UObject/UObjectGlobals.h"

UPRGameInstance::UPRGameInstance()
	: SessionInterfaceState(EPRSessionInterfaceState::Idle)
	, LastRequestedPublicConnections(4)
	, bLastRequestedLANMatch(true)
{
}

void UPRGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPRGameInstance::HandlePostLoadMapWithWorld);

	UE_LOG(LogProjectA, Log, TEXT("ProjectRift game instance initialized."));
}

void UPRGameInstance::OnStart()
{
	Super::OnStart();

	// The initial PIE/game map can finish loading before PostLoadMapWithWorld is
	// observed by this game instance. Reconcile the already-active world once the
	// game has started; the widget guard keeps this idempotent with the delegate.
	HandlePostLoadMapWithWorld(GetWorld());
}

void UPRGameInstance::Shutdown()
{
	if (UPRSaveSubsystem* SaveSubsystem = GetSubsystem<UPRSaveSubsystem>())
	{
		SaveSubsystem->SaveForApplicationExit();
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (ProfileDebugWidget)
	{
		ProfileDebugWidget->RemoveFromParent();
		ProfileDebugWidget = nullptr;
	}
	Super::Shutdown();
}

void UPRGameInstance::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!LoadedWorld || LoadedWorld != GetWorld())
	{
		return;
	}
	const bool bIsMainMenu = LoadedWorld->GetMapName().EndsWith(TEXT("L_MainMenu"));
	if (!bIsMainMenu)
	{
		if (ProfileDebugWidget)
		{
			ProfileDebugWidget->RemoveFromParent();
			ProfileDebugWidget = nullptr;
			if (APlayerController* Controller = LoadedWorld->GetFirstPlayerController())
			{
				Controller->bShowMouseCursor = false;
				Controller->SetInputMode(FInputModeGameOnly());
			}
		}
		return;
	}
	if (!ProfileDebugWidget)
	{
		ProfileDebugWidget = CreateWidget<UPRProfileDebugWidget>(this, UPRProfileDebugWidget::StaticClass());
		if (ProfileDebugWidget)
		{
			ProfileDebugWidget->AddToViewport(100);
			ProfileDebugWidget->SetPositionInViewport(FPRDebugUILayout::ProfilePosition(), false);
			ProfileDebugWidget->SetDesiredSizeInViewport(FPRDebugUILayout::ProfileSize());
			ProfileDebugWidget->SetAnchorsInViewport(FPRDebugUILayout::ProfileAnchors());
			ProfileDebugWidget->SetAlignmentInViewport(FPRDebugUILayout::ProfileAlignment());
			if (APlayerController* Controller = LoadedWorld->GetFirstPlayerController())
			{
				Controller->bShowMouseCursor = true;
				FInputModeUIOnly InputMode;
				InputMode.SetWidgetToFocus(ProfileDebugWidget->TakeWidget());
				Controller->SetInputMode(InputMode);
			}
		}
	}
#endif
}

bool UPRGameInstance::CreateSession(const int32 PublicConnections, const bool bIsLANMatch)
{
	LastSessionError.Reset();
	CachedSessionSearchResults.Reset();
	LastRequestedPublicConnections = FMath::Max(1, PublicConnections);
	bLastRequestedLANMatch = bIsLANMatch;

	SetSessionInterfaceState(EPRSessionInterfaceState::Creating);
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("CreateSession requested. PublicConnections=%d LAN=%s"),
		LastRequestedPublicConnections,
		bLastRequestedLANMatch ? TEXT("true") : TEXT("false"));

	// Placeholder until the real OnlineSubsystem session implementation is added.
	SetSessionInterfaceState(EPRSessionInterfaceState::InSession);
	return true;
}

bool UPRGameInstance::FindSessions(const int32 MaxSearchResults, const bool bIsLANQuery)
{
	LastSessionError.Reset();
	CachedSessionSearchResults.Reset();

	SetSessionInterfaceState(EPRSessionInterfaceState::Searching);
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("FindSessions requested. MaxSearchResults=%d LAN=%s"),
		FMath::Max(1, MaxSearchResults),
		bIsLANQuery ? TEXT("true") : TEXT("false"));

	// Placeholder search keeps an empty cache until OnlineSubsystemNull/Steam is wired in.
	SetSessionInterfaceState(EPRSessionInterfaceState::Idle);
	return true;
}

bool UPRGameInstance::JoinSession(const FString& SessionId)
{
	LastSessionError.Reset();

	if (SessionId.TrimStartAndEnd().IsEmpty())
	{
		SetLastSessionError(TEXT("SessionId is empty."));
		SetSessionInterfaceState(EPRSessionInterfaceState::Error);
		UE_LOG(LogProjectA, Warning, TEXT("JoinSession rejected: %s"), *LastSessionError);
		return false;
	}

	SetSessionInterfaceState(EPRSessionInterfaceState::Joining);
	UE_LOG(LogProjectA, Log, TEXT("JoinSession requested. SessionId=%s"), *SessionId);

	// Placeholder until a real session result can be resolved by OnlineSubsystem.
	SetSessionInterfaceState(EPRSessionInterfaceState::InSession);
	return true;
}

bool UPRGameInstance::DestroySession()
{
	LastSessionError.Reset();
	CachedSessionSearchResults.Reset();
	if (UPRSaveSubsystem* SaveSubsystem = GetSubsystem<UPRSaveSubsystem>())
	{
		SaveSubsystem->ReleaseSessionProfileBinding();
	}

	SetSessionInterfaceState(EPRSessionInterfaceState::Destroying);
	UE_LOG(LogProjectA, Log, TEXT("DestroySession requested."));

	SetSessionInterfaceState(EPRSessionInterfaceState::Idle);
	return true;
}

bool UPRGameInstance::StartSession()
{
	LastSessionError.Reset();

	if (SessionInterfaceState != EPRSessionInterfaceState::InSession)
	{
		SetLastSessionError(TEXT("No active placeholder session to start."));
		SetSessionInterfaceState(EPRSessionInterfaceState::Error);
		UE_LOG(LogProjectA, Warning, TEXT("StartSession rejected: %s"), *LastSessionError);
		return false;
	}

	UE_LOG(LogProjectA, Log, TEXT("StartSession requested."));
	return true;
}

void UPRGameInstance::SetSessionInterfaceState(const EPRSessionInterfaceState NewState)
{
	SessionInterfaceState = NewState;
}

void UPRGameInstance::SetLastSessionError(const FString& ErrorMessage)
{
	LastSessionError = ErrorMessage;
}
