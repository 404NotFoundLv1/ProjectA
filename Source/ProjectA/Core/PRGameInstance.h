#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PRGameInstance.generated.h"

class UPRProfileDebugWidget;

UENUM(BlueprintType)
enum class EPRSessionInterfaceState : uint8
{
	Idle,
	Creating,
	Searching,
	Joining,
	InSession,
	Destroying,
	Error
};

USTRUCT(BlueprintType)
struct FPRSessionSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Session")
	bool bIsLANMatch = true;
};

/**
 * Project-level runtime owner for menus, sessions, and cross-map state.
 */
UCLASS()
class PROJECTA_API UPRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPRGameInstance();

	using UGameInstance::JoinSession;

	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool CreateSession(int32 PublicConnections = 4, bool bIsLANMatch = true);

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool FindSessions(int32 MaxSearchResults = 20, bool bIsLANQuery = true);

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool JoinSession(const FString& SessionId);

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool DestroySession();

	UFUNCTION(BlueprintCallable, Category = "Session")
	bool StartSession();

	UFUNCTION(BlueprintPure, Category = "Session")
	EPRSessionInterfaceState GetSessionInterfaceState() const { return SessionInterfaceState; }

	UFUNCTION(BlueprintPure, Category = "Session")
	FString GetLastSessionError() const { return LastSessionError; }

	UFUNCTION(BlueprintPure, Category = "Session")
	TArray<FPRSessionSearchResult> GetCachedSessionSearchResults() const { return CachedSessionSearchResults; }

private:
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void SetSessionInterfaceState(EPRSessionInterfaceState NewState);
	void SetLastSessionError(const FString& ErrorMessage);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Session", meta = (AllowPrivateAccess = "true"))
	EPRSessionInterfaceState SessionInterfaceState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Session", meta = (AllowPrivateAccess = "true"))
	TArray<FPRSessionSearchResult> CachedSessionSearchResults;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Session", meta = (AllowPrivateAccess = "true"))
	FString LastSessionError;

	int32 LastRequestedPublicConnections;

	bool bLastRequestedLANMatch;

	UPROPERTY(Transient)
	TObjectPtr<UPRProfileDebugWidget> ProfileDebugWidget;
};
