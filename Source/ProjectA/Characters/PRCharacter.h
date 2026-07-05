#pragma once

#include "CoreMinimal.h"
#include "ProjectACharacter.h"
#include "PRCharacter.generated.h"

class UTextRenderComponent;

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

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network|Debug")
	UTextRenderComponent* PlayerDebugLabel;

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
