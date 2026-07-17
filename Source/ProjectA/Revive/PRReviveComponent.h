#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Revive/PRReviveTypes.h"
#include "PRReviveComponent.generated.h"

class APRCharacter;

/** Server-authoritative downed, bleed-out, and rescue state for one playable character. */
UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRReviveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRReviveComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EnterDownedState();
	bool BeginPlayerRevive(APRCharacter* Rescuer);
	bool BeginDroneRevive();
	bool CompleteRevive(EPRReviveSource RequestedSource);
	void CancelRevive(APRCharacter* RequestingRescuer = nullptr);
	void CancelForMissionEnd();

	UFUNCTION(BlueprintPure, Category = "Revive")
	APRCharacter* GetCurrentReviver() const { return CurrentReviver; }

	UFUNCTION(BlueprintPure, Category = "Revive")
	EPRReviveSource GetReviveSource() const { return ReviveSource; }

	UFUNCTION(BlueprintPure, Category = "Revive")
	bool IsReviveInProgress() const { return ReviveSource != EPRReviveSource::None; }

	UFUNCTION(BlueprintPure, Category = "Revive")
	float GetBleedOutRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Revive")
	float GetReviveProgress() const;

	UFUNCTION(BlueprintPure, Category = "Revive")
	bool CanBeRevivedBy(const APRCharacter* Rescuer) const;

private:
	APRCharacter* GetOwnerCharacter() const;
	bool IsOwnerDownedAndNotDead() const;
	bool ValidatePlayerRevive(APRCharacter* Rescuer) const;
	bool HasReviveLineOfSight(const APRCharacter* Rescuer) const;
	bool BeginRevive(APRCharacter* Rescuer, EPRReviveSource InSource, float Duration);
	void ClearReviveState();
	void CompleteBleedOut();
	float GetServerTimeSeconds() const;

	UFUNCTION()
	void HandleReviverHealthChanged(float OldValue, float NewValue);

	UFUNCTION()
	void HandleReviverShieldChanged(float OldValue, float NewValue);

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive")
	float BleedOutEndServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive")
	float ReviveStartServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive")
	float ReviveDuration = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive")
	TObjectPtr<APRCharacter> CurrentReviver;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Revive")
	EPRReviveSource ReviveSource = EPRReviveSource::None;

	FVector ReviverStartLocation = FVector::ZeroVector;
};
