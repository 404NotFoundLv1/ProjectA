#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/PRCombatUnitInterface.h"
#include "GameplayTagContainer.h"
#include "ProjectACharacter.h"
#include "PRCharacter.generated.h"

class APRPlayerState;
class UGameplayAbility;
class UGameplayEffect;
class UPRAbilitySystemComponent;
class UPRAttributeSet;
class UPRCombatFeedbackComponent;
class UPRReviveComponent;
class UTextRenderComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRAbilitySystemInitializedSignature, UPRAbilitySystemComponent*, AbilitySystemComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPRAttributeChangedSignature, float, OldValue, float, NewValue);

/**
 * Base playable avatar for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRCharacter : public AProjectACharacter, public IAbilitySystemInterface, public IPRCombatUnitInterface
{
	GENERATED_BODY()

public:
	APRCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual bool IsCombatUnitInactive() const override;
	virtual void HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext) override;

	UFUNCTION(BlueprintCallable, Category = "Network|Debug")
	void RefreshPlayerDebugLabel();

	UTextRenderComponent* GetPlayerDebugLabel() const { return PlayerDebugLabel; }

	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool InitializeAbilitySystemFromPlayerState(APRPlayerState* InPlayerState);

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAttributeSet* GetAttributeSet() const { return AttributeSet.Get(); }

	UFUNCTION(BlueprintPure, Category = "Revive")
	UPRReviveComponent* GetReviveComponent() const { return ReviveComponent; }

	UFUNCTION(BlueprintPure, Category = "GAS")
	bool IsAbilitySystemInitialized() const { return bAbilitySystemInitialized; }

	UFUNCTION(BlueprintCallable, Category = "GAS")
	bool ApplyDefaultAttributes();

	UFUNCTION(BlueprintPure, Category = "GAS")
	bool AreDefaultAttributesApplied() const { return bDefaultAttributesApplied; }

	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	bool GrantDefaultAbilities();

	UFUNCTION(BlueprintPure, Category = "GAS|Abilities")
	bool AreDefaultAbilitiesGranted() const { return bDefaultAbilitiesGranted; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GAS|Abilities")
	bool GrantSelectedRoleModuleAbilities();

	UFUNCTION(BlueprintPure, Category = "GAS|Abilities")
	bool AreRoleModuleAbilitiesGranted() const { return bRoleModuleAbilitiesGranted; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Life State")
	bool EnterDownedState();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Life State")
	bool RespawnFromDowned();

	UFUNCTION(BlueprintPure, Category = "Life State")
	bool IsDowned() const;

	UFUNCTION(BlueprintPure, Category = "Life State")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Life State")
	bool IsAutoRespawnScheduled() const;

	UPROPERTY(BlueprintAssignable, Category = "GAS")
	FPRAbilitySystemInitializedSignature OnAbilitySystemInitialized;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FPRAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FPRAttributeChangedSignature OnShieldChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FPRAttributeChangedSignature OnEnergyChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void BindAttributeChangeDelegates();
	void ClearAttributeChangeDelegates();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);
	void HandleStunnedTagChanged(FGameplayTag StatusTag, int32 NewCount);
	void HandleHitStaggeredTagChanged(FGameplayTag StatusTag, int32 NewCount);
	void RefreshMovementFromAttributes();
	bool GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, FGameplayTag InputTag);
	bool ActivateAbilityInputTag(FGameplayTag InputTag, const TCHAR* ActionName);
	bool ReleaseAbilityInputTag(FGameplayTag InputTag, const TCHAR* ActionName);
	FTransform BuildLocalDeployablePreviewTransform() const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	bool bAbilitySystemInitialized;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	bool bDefaultAttributesApplied;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	bool bDefaultAbilitiesGranted;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	bool bRoleModuleAbilitiesGranted;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Defaults")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilityClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities|Assault")
	TSubclassOf<UGameplayAbility> AssaultChargeAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities|Assault")
	TSubclassOf<UGameplayAbility> AssaultBlastAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Abilities|Assault")
	TSubclassOf<UGameplayAbility> AssaultShieldAbilityClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network|Debug")
	UTextRenderComponent* PlayerDebugLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Feedback")
	TObjectPtr<UPRCombatFeedbackComponent> CombatFeedbackComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Revive")
	TObjectPtr<UPRReviveComponent> ReviveComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network|Debug")
	bool bShowPlayerDebugLabel;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle EnergyChangedDelegateHandle;
	FDelegateHandle MoveSpeedChangedDelegateHandle;
	FDelegateHandle StunnedTagChangedDelegateHandle;
	FDelegateHandle HitStaggeredTagChangedDelegateHandle;
	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* PrimaryAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* AimAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Actions")
	UInputAction* ReloadAction;

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
	void DoInteractReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoPrimaryAttack();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoPrimaryAttackReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoAim();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoAimReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoReload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ApplyWeaponAimPresentation(bool bNewAiming);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoDodge();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillQ();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillQReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillE();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillEReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillR();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoSkillRReleased();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoOpenInventory();

private:
	friend class UPRReviveComponent;
	float DefaultCameraArmLength = 400.0f;
	float DefaultCameraFieldOfView = 90.0f;
	float TargetCameraArmLength = 400.0f;
	float TargetCameraFieldOfView = 90.0f;
	bool bDefaultUseControllerRotationYaw = false;
	bool bDefaultOrientRotationToMovement = true;
	bool bWeaponAimPresentationActive = false;
};
