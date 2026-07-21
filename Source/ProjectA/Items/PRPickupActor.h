#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/PRItemTypes.h"
#include "Items/PRRewardTypes.h"
#include "PRPickupActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UPrimitiveComponent;
class APawn;

/**
 * Replicated world item actor used as the visual/interactable pickup target.
 */
UCLASS(Blueprintable)
class PROJECTA_API APRPickupActor : public AActor
{
	GENERATED_BODY()

public:
	APRPickupActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	FPRItemInstance GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void SetItemInstance(const FPRItemInstance& InItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup|Rewards")
	void SetRewardSource(const FPRRewardSourceContext& InRewardSource);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup|Objective")
	void SetObjectiveNodeId(FName InObjectiveNodeId) { ObjectiveNodeId = InObjectiveNodeId; ForceNetUpdate(); }

	UFUNCTION(BlueprintPure, Category = "Pickup|Objective")
	FName GetObjectiveNodeId() const { return ObjectiveNodeId; }

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool CanBePickedUp() const;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsPickedUp() const { return bIsPickedUp; }

	UFUNCTION(BlueprintPure, Category = "Pickup")
	FText GetInteractionPromptText() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void SetPickedUp(bool bNewPickedUp);

	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }
	UWidgetComponent* GetInteractionPromptWidget() const { return InteractionPromptWidget; }

private:
	UFUNCTION()
	void OnRep_ItemInstance();

	UFUNCTION()
	void OnRep_IsPickedUp();

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

	void RefreshPickupVisualState();
	void SetInteractionPromptVisible(bool bVisible);
	void RefreshInteractionPromptWidget();
	void UpdateInteractionPromptVisibility();
	APawn* FindNearbyPromptPawn() const;
	void UpdateInteractionPromptPlacement(const APawn* NearbyPawn);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

	UPROPERTY(ReplicatedUsing = OnRep_ItemInstance, EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	FPRItemInstance ItemInstance;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pickup|Rewards", meta = (AllowPrivateAccess = "true"))
	FPRRewardSourceContext RewardSource;

	UPROPERTY(Replicated, EditInstanceOnly, BlueprintReadOnly, Category = "Pickup|Objective", meta = (AllowPrivateAccess = "true"))
	FName ObjectiveNodeId;

	UPROPERTY(ReplicatedUsing = OnRep_IsPickedUp, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	bool bIsPickedUp;
};
