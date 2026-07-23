#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRBossEncounterController.generated.h"

class APRBossCharacter;
class UPRBossDefinitionDataAsset;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRBossEncounterDefeatedSignature, APRBossCharacter*, DefeatedBoss);

/** Owns a reusable boss attempt and its recovery boundary; it intentionally has no per-frame Tick. */
UCLASS(BlueprintType)
class PROJECTA_API APRBossEncounterController : public AActor
{
	GENERATED_BODY()

public:
	APRBossEncounterController();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") bool StartBossEncounter();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") void ResetBossEncounter();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") void AbortBossEncounter();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") void RecoverBossToAnchor();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") void HandleEncounterWipe();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") void NotifyBossDefeated(APRBossCharacter* DefeatedBoss);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss|Encounter") FName ConsumePendingRewardId();
	UFUNCTION(BlueprintPure, Category = "Boss|Encounter") APRBossCharacter* GetActiveBoss() const { return ActiveBoss; }
	UPROPERTY(BlueprintAssignable, Category = "Boss|Encounter") FPRBossEncounterDefeatedSignature OnBossDefeated;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TObjectPtr<UPRBossDefinitionDataAsset> BossDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TSubclassOf<APRBossCharacter> BossClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") bool bAutoStart = false;
	/** Lets initial network clients register before an auto-started test encounter freezes its party size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta=(ClampMin="0.0")) float AutoStartDelaySeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") FTransform BossAnchor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Recovery") bool bResetOnFullWipe = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Recovery", meta=(ClampMin="0.0")) float DisconnectGraceSeconds = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Recovery", meta=(ClampMin="0.0")) float ArenaRecoveryRadius = 6000.0f;
	UPROPERTY(Transient) TObjectPtr<APRBossCharacter> ActiveBoss;
	UPROPERTY(Transient) FName PendingRewardId = NAME_None;

private:
	void ClearBossOwnedActors();
	void StartAutoBossEncounter();
	void MonitorRecovery();
	int32 CountConnectedPlayers() const;
	bool HasConnectedPlayer() const;
	bool HasLivingPlayer() const;
	FTimerHandle AutoStartTimer;
	FTimerHandle RecoveryTimer;
	float NoPlayerSince = -1.0f;
};
