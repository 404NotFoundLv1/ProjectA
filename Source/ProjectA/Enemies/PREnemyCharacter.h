#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PREnemyCharacter.generated.h"

class APRPickupActor;
class UPRLootTableDataAsset;
class UWidgetComponent;

/**
 * First-pass replicated melee enemy for rift objectives.
 */
UCLASS()
class PROJECTA_API APREnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APREnemyCharacter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool ApplyEnemyDamage(float DamageAmount, AController* DamageInstigator);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Combat")
	bool TryMeleeAttack(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Loot")
	APRPickupActor* SpawnDeathLoot();

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Combat")
	float GetAttackDamage() const { return AttackDamage; }

protected:
	void HandleDeath(AController* DeathInstigator);
	void ApplyDeathState();
	bool ApplyDamageToProjectRiftCharacter(AActor* TargetActor);
	void InitializeHealthBarWidget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Visual")
	TObjectPtr<UWidgetComponent> EnemyHealthBarWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|State", meta = (ClampMin = "1.0"))
	float MaxHealth = 50.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Enemy|State")
	float Health = 50.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly, Category = "Enemy|State")
	bool bDead = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 220.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	TObjectPtr<UPRLootTableDataAsset> DeathLootTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	TSubclassOf<APRPickupActor> PickupActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Loot")
	float DeathLootRollOverride = 0.0f;

private:
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Dead();

	UPROPERTY(Transient)
	bool bDeathLootSpawned = false;
};
