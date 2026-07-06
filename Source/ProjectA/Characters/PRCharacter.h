#pragma once

#include "CoreMinimal.h"
#include "ProjectACharacter.h"
#include "PRCharacter.generated.h"

class APRPlayerState;
class UPRAbilitySystemComponent;
class UPRAttributeSet;
class UTextRenderComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRAbilitySystemInitializedSignature, UPRAbilitySystemComponent*, AbilitySystemComponent);

/**
 * Base playable avatar for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRCharacter : public AProjectACharacter
{
	GENERATED_BODY()

public:
	APRCharacter();

	UFUNCTION(BlueprintCallable, Category = "Network|Debug")
	void RefreshPlayerDebugLabel();

	UTextRenderComponent* GetPlayerDebugLabel() const { return PlayerDebugLabel; }

	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool InitializeAbilitySystemFromPlayerState(APRPlayerState* InPlayerState);

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAttributeSet* GetAttributeSet() const { return AttributeSet.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS")
	bool IsAbilitySystemInitialized() const { return bAbilitySystemInitialized; }

	UPROPERTY(BlueprintAssignable, Category = "GAS")
	FPRAbilitySystemInitializedSignature OnAbilitySystemInitialized;

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void BindAttributeChangeDelegates();
	void ClearAttributeChangeDelegates();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	bool bAbilitySystemInitialized;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network|Debug")
	UTextRenderComponent* PlayerDebugLabel;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle EnergyChangedDelegateHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* PrimaryAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* DodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* SkillQAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* SkillEAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* SkillRAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* OpenInventoryAction;

public:
	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoInteract();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoDodge();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillQ();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillE();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillR();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoOpenInventory();
};
