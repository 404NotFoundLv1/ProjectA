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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetReady(bool bReady);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FName GetSelectedRoleModule() const { return SelectedRoleModule; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetSelectedRoleModule(FName InSelectedRoleModule);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FString GetPlayerDisplayName() const { return PlayerDisplayName; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetPlayerDisplayName(const FString& InPlayerDisplayName);

private:
	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_PlayerDisplayName();

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsReady;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FName SelectedRoleModule;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerDisplayName, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FString PlayerDisplayName;
};
