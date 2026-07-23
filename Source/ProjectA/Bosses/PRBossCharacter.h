#pragma once

#include "Enemies/PREnemyCharacter.h"
#include "PRBossCharacter.generated.h"

class APRBossEncounterController;
class UPRBossDefinitionDataAsset;
class UPRBossSchedulerComponent;
class UPRBossWeakPointComponent;
class USphereComponent;
class UPrimitiveComponent;
struct FPRBossPhaseDefinition;

/** A GAS enemy specialized for data-driven boss scheduling. */
UCLASS()
class PROJECTA_API APRBossCharacter : public APREnemyCharacter
{
	GENERATED_BODY()

public:
	APRBossCharacter();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext) override;
	virtual void HandleCombatUnitDamageResolved(AActor* DamageSource, float ResolvedDamage, const FGameplayEffectContextHandle& EffectContext) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Boss")
	bool InitializeBoss(UPRBossDefinitionDataAsset* InDefinition, APRBossEncounterController* InEncounterController, int32 InFrozenPlayerCount);
	UFUNCTION(BlueprintPure, Category = "Boss") UPRBossDefinitionDataAsset* GetBossDefinition() const { return BossDefinition; }
	UFUNCTION(BlueprintPure, Category = "Boss") UPRBossSchedulerComponent* GetBossScheduler() const { return BossScheduler; }
	UFUNCTION(BlueprintPure, Category = "Boss") FName GetBossDefinitionId() const { return BossDefinitionId; }
	bool IsWeakPointHitComponent(const UPrimitiveComponent* HitComponent) const;
	float GetWeakPointDamageMultiplier(const UPrimitiveComponent* HitComponent) const;
	bool IsWeakPointExposed() const;
	void ApplyPhaseModifiers(const FPRBossPhaseDefinition& PhaseDefinition);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TObjectPtr<UPRBossSchedulerComponent> BossScheduler;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TObjectPtr<UPRBossWeakPointComponent> DefaultWeakPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TObjectPtr<USphereComponent> WeakPointCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TArray<TObjectPtr<UPRBossWeakPointComponent>> AdditionalWeakPoints;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss") TArray<TObjectPtr<USphereComponent>> AdditionalWeakPointCollisions;
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss") FName BossDefinitionId;
	UPROPERTY(Transient) TObjectPtr<UPRBossDefinitionDataAsset> BossDefinition;
	UPROPERTY(Transient) TWeakObjectPtr<APRBossEncounterController> EncounterController;
	int32 FrozenPlayerCount = 1;
	const UPRBossWeakPointComponent* FindWeakPoint(const UPrimitiveComponent* HitComponent) const;
};
