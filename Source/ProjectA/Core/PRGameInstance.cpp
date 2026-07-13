#include "Core/PRGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Diagnostics/PRDiagnosticsLog.h"
#include "GameFramework/PlayerController.h"
#include "Persistence/PRSaveSubsystem.h"
#include "ProjectA.h"
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

	UE_LOG(LogProjectRiftNetwork, Log, TEXT("ProjectRift game instance initialized."));
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
	Super::Shutdown();
}

void UPRGameInstance::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
#if UE_BUILD_SHIPPING
	return;
#else
	// The legacy profile debug widget remains available as a class for compatibility,
	// but v0.5.5 no longer opens any scattered debug HUD from the game instance.
	(void)LoadedWorld;
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
	const EPRSessionInterfaceState PreviousState = SessionInterfaceState;
	SessionInterfaceState = NewState;
	PRRecordDiagnosticEvent(
		this,
		NewState == EPRSessionInterfaceState::Error ? EPRDiagnosticSeverity::Error : EPRDiagnosticSeverity::Info,
		TEXT("Network"),
		TEXT("Session.StateChanged"),
		FString::Printf(
			TEXT("Session state changed from %s to %s."),
			*StaticEnum<EPRSessionInterfaceState>()->GetNameStringByValue(static_cast<int64>(PreviousState)),
			*StaticEnum<EPRSessionInterfaceState>()->GetNameStringByValue(static_cast<int64>(NewState))));
}

void UPRGameInstance::SetLastSessionError(const FString& ErrorMessage)
{
	LastSessionError = ErrorMessage;
	if (!LastSessionError.IsEmpty())
	{
		UE_LOG(LogProjectRiftNetwork, Warning, TEXT("Session error: %s"), *LastSessionError);
		PRRecordDiagnosticEvent(
			this,
			EPRDiagnosticSeverity::Error,
			TEXT("Network"),
			TEXT("Session.Error"),
			LastSessionError);
	}
}
