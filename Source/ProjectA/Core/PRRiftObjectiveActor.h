#pragma once

#include "CoreMinimal.h"
#include "Core/PRRiftGameState.h"
#include "GameFramework/Actor.h"
#include "PRRiftObjectiveActor.generated.h"

class APRRiftGameMode;
class USphereComponent;
class UWidgetComponent;
class UPrimitiveComponent;
class APawn;

/**
 * Base replicated mission objective actor for rift maps.
 */
UCLASS()
class PROJECTA_API APRRiftObjectiveActor : public AActor
{
	GENERATED_BODY()

public:
	APRRiftObjectiveActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	virtual bool ActivateObjective(AController* ActivatingController);

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	virtual bool CanActivateObjective(AController* ActivatingController) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Objective")
	virtual void CompleteObjective();

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	bool IsObjectiveActive() const { return ObjectiveState == EPRRiftObjectiveState::Active; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	bool IsObjectiveCompleted() const { return ObjectiveState == EPRRiftObjectiveState::Completed; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	EPRRiftObjectiveState GetObjectiveState() const { return ObjectiveState; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	float GetObjectiveProgress() const { return ObjectiveProgress; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	float GetInteractionRadius() const { return InteractionRadius; }

	UFUNCTION(BlueprintPure, Category = "Rift|Objective")
	FText GetInteractionPromptText() const;

	UWidgetComponent* GetInteractionPromptWidget() const { return InteractionPromptWidget; }

protected:
	virtual void HandleObjectiveActivated(AController* ActivatingController);
	virtual void HandleObjectiveCompleted();

	void SetObjectiveState(EPRRiftObjectiveState InObjectiveState);
	void SetObjectiveProgress(float InObjectiveProgress);
	APRRiftGameMode* GetRiftGameMode() const;
	void RefreshInteractionPromptText();
	void SetInteractionPromptVisible(bool bVisible);
	void RefreshInteractionPromptWidget();
	FLinearColor GetInteractionPromptColor() const;
	void UpdateInteractionPromptVisibility();
	APawn* FindNearbyPromptPawn() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Objective")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Objective")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Objective")
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rift|Objective", meta = (ClampMin = "0.0"))
	float InteractionRadius = 250.0f;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveState, BlueprintReadOnly, Category = "Rift|Objective")
	EPRRiftObjectiveState ObjectiveState = EPRRiftObjectiveState::NotStarted;

	UPROPERTY(ReplicatedUsing = OnRep_ObjectiveProgress, BlueprintReadOnly, Category = "Rift|Objective")
	float ObjectiveProgress = 0.0f;

	UFUNCTION()
	void OnRep_ObjectiveState();

	UFUNCTION()
	void OnRep_ObjectiveProgress();

	UFUNCTION()
	void HandleInteractionSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
