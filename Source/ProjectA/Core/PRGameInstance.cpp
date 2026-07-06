#include "Core/PRGameInstance.h"

#include "ProjectA.h"

UPRGameInstance::UPRGameInstance()
	: SessionInterfaceState(EPRSessionInterfaceState::Idle)
	, LastRequestedPublicConnections(4)
	, bLastRequestedLANMatch(true)
{
}

void UPRGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogProjectA, Log, TEXT("ProjectRift game instance initialized."));
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
