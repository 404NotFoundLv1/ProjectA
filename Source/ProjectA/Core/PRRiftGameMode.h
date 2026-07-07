#pragma once

#include "CoreMinimal.h"
#include "Core/PRGameModeBase.h"
#include "Core/PRRiftGameState.h"
#include "PRRiftGameMode.generated.h"

/**
 * Server-authoritative rule set for rift missions.
 */
UCLASS()
class PROJECTA_API APRRiftGameMode : public APRGameModeBase
{
	GENERATED_BODY()

public:
	APRRiftGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	bool StartRiftMission();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void CompleteCurrentObjective();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void OpenExtraction();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Mission")
	void UpdateAlivePlayerCount();

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	bool HasRiftMissionStarted() const { return bRiftMissionStarted; }

protected:
	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	APRRiftGameState* GetRiftGameState() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Mission")
	int32 CountAlivePlayers() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Mission", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialRiftStability = 100.0f;

private:
	bool bRiftMissionStarted = false;
};
