#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/PRCombatUnitInterface.h"
#include "GameFramework/Character.h"
#include "PREnemyCharacter.generated.h"

class APRPickupActor;
class UGameplayAbility;
class UPRAbilitySystemComponent;
class UPRAttributeSet;
class UPRCombatFeedbackComponent;
class UPREnemyDefinitionDataAsset;
class UPREnemyBehaviorComponent;
class UPREnemyThreatComponent;
class UPRLootTableDataAsset;
class UWidgetComponent;
struct FOnAttributeChangeData;

/**
 * First-pass replicated melee enemy for rift objectives.
 */
UCLASS()
class PROJECTA_API APREnemyCharacter : public ACharacter, public IAbilitySystemInterface, public IPRCombatUnitInterface
{
	GENERATED_BODY()

public:
	APREnemyCharacter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool IsCombatUnitInactive() const override;
	virtual void HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext) override;
	virtual void HandleCombatUnitDamageResolved(AActor* DamageSource, float ResolvedDamage) override;

	UFUNCTION(BlueprintPure, Category = "Enemy|GAS")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|GAS")
	UPRAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool ApplyEnemyDamage(float DamageAmount, AController* DamageInstigator);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool TryMeleeAttack(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Loot")
	APRPickupActor* SpawnDeathLoot();

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	FString GetActiveStatusText() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackDamage() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Scaling")
	void SetSpawnHealthMultiplier(float InMultiplier);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Scaling")
	void SetSpawnAttackPowerMultiplier(float InMultiplier);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Definition")
	bool SetEnemyDefinition(UPREnemyDefinitionDataAsset* InDefinition);

	UFUNCTION(BlueprintPure, Category = "Enemy|Definition")
	FName GetEnemyDefinitionId() const { return EnemyDefinitionId; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Definition")
	bool IsStatusImmune(FGameplayTag StatusTag) const;
	bool IsHeavyHitReactionsOnly() const;

	UPREnemyDefinitionDataAsset* GetEnemyDefinition() const { return EnemyDefinition; }
	UPREnemyThreatComponent* GetEnemyThreatComponent() const { return EnemyThreatComponent; }
	UPREnemyBehaviorComponent* GetEnemyBehaviorComponent() const { return EnemyBehaviorComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Objective")
	void SetHuntTargetId(FName InHuntTargetId) { HuntTargetId = InHuntTargetId; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Objective")
	FName GetHuntTargetId() const { return HuntTargetId; }

protected:
	void HandleDeath(AController* DeathInstigator);
	void ApplyDeathState();
	void InitializeHealthBarWidget();
	void BindCombatDelegates();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void HandleStunnedTagChanged(const FGameplayTag StatusTag, int32 NewCount);
	void HandleHitStaggeredTagChanged(const FGameplayTag StatusTag, int32 NewCount);
	void HandleDeadTagChanged(const FGameplayTag StatusTag, int32 NewCount);
	void RefreshMovementFromAttributes();
	UFUNCTION() void OnRep_EnemyDefinitionId();
	void ResolveEnemyDefinitionFromId();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Visual")
	TObjectPtr<UWidgetComponent> EnemyHealthBarWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|GAS")
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Combat|Feedback")
	TObjectPtr<UPRCombatFeedbackComponent> CombatFeedbackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<UPREnemyThreatComponent> EnemyThreatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<UPREnemyBehaviorComponent> EnemyBehaviorComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|State", meta = (ClampMin = "1.0"))
	float MaxHealth = 50.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use GetHealth; GAS attributes are authoritative."))
	float Health = 50.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use IsDead; GAS state tags are authoritative."))
	bool bDead = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat")
	TSubclassOf<UGameplayAbility> EnemyMeleeAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	TObjectPtr<UPRLootTableDataAsset> DeathLootTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	TSubclassOf<APRPickupActor> PickupActorClass;

	/** Optional legacy deterministic roll for focused tests. Negative values use the server-owned LootSeed path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	float DeathLootRollOverride = -1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Scaling")
	float SpawnHealthMultiplier = 1.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Scaling")
	float SpawnAttackPowerMultiplier = 1.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Objective")
	FName HuntTargetId;

	UPROPERTY(ReplicatedUsing=OnRep_EnemyDefinitionId, VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Definition")
	FName EnemyDefinitionId;

	UPROPERTY(Transient)
	TObjectPtr<UPREnemyDefinitionDataAsset> EnemyDefinition;

private:
	UPROPERTY(Transient)
	bool bDeathLootSpawned = false;

	UPROPERTY(Transient)
	bool bDeathHandled = false;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;
	FDelegateHandle HitStaggeredTagChangedDelegateHandle;
	FDelegateHandle DeadTagChangedDelegateHandle;
};
