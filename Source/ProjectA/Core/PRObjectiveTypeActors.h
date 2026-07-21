#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/PRCombatUnitInterface.h"
#include "Core/PRRiftObjectiveActor.h"
#include "PRObjectiveTypeActors.generated.h"

class UAbilitySystemComponent;
class UPRAbilitySystemComponent;
class UPRAttributeSet;

/** Collects distinct quest-item identities for a graph node. */
UCLASS()
class PROJECTA_API APRCollectObjectiveActor : public APRRiftObjectiveActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective|Collect")
	bool RegisterCollectedItem(FGuid ItemInstanceGuid);

	bool HasCollectedItem(const FGuid ItemInstanceGuid) const { return ItemInstanceGuid.IsValid() && CollectedItemGuids.Contains(ItemInstanceGuid); }

private:
	TSet<FGuid> CollectedItemGuids;
};

/** Accepts exactly the tracked core identity at a graph delivery point. */
UCLASS()
class PROJECTA_API APRCarryObjectiveActor : public APRRiftObjectiveActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective|Carry")
	bool SetTrackedCoreGuid(FGuid InCoreGuid);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective|Carry")
	bool SubmitTrackedCore(FGuid CoreGuid);

	bool IsTrackedCoreGuid(const FGuid CoreGuid) const { return CoreGuid.IsValid() && CoreGuid == TrackedCoreGuid; }
	const FGuid& GetTrackedCoreGuid() const { return TrackedCoreGuid; }

private:
	FGuid TrackedCoreGuid;
};

/** Server callback target for a destroyable graph target after its GAS health reaches zero. */
UCLASS()
class PROJECTA_API APRDestroyObjectiveActor : public APRRiftObjectiveActor, public IAbilitySystemInterface, public IPRCombatUnitInterface
{
	GENERATED_BODY()

public:
	APRDestroyObjectiveActor();
	virtual void BeginPlay() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool IsCombatUnitInactive() const override;
	virtual void HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext) override;

	UFUNCTION(BlueprintPure, Category = "Rift|Objective|Destroy")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective|Destroy")
	bool RegisterDestroyedTarget();

private:
	UPROPERTY(VisibleAnywhere, Category = "Rift|Objective|Destroy")
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rift|Objective|Destroy")
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(EditInstanceOnly, Category = "Rift|Objective|Destroy", meta = (ClampMin = "1.0"))
	float TargetHealth = 120.0f;

	bool bTargetDestroyed = false;
};

/** Server callback target for kills marked with this hunt graph node's TargetId. */
UCLASS()
class PROJECTA_API APRHuntObjectiveActor : public APRRiftObjectiveActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective|Hunt")
	bool RegisterHuntTargetEliminated(FName TargetId);
};
