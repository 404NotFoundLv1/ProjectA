#pragma once

#include "CoreMinimal.h"
#include "ProjectACharacter.h"
#include "PRCharacter.generated.h"

/**
 * Base playable avatar for ProjectRift.
 */
UCLASS()
class PROJECTA_API APRCharacter : public AProjectACharacter
{
	GENERATED_BODY()

public:
	APRCharacter();

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

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
