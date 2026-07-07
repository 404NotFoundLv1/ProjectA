#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/PRItemTypes.h"
#include "PRPickupActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Replicated world item actor used as the visual/interactable pickup target.
 */
UCLASS(Blueprintable)
class PROJECTA_API APRPickupActor : public AActor
{
	GENERATED_BODY()

public:
	APRPickupActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	FPRItemInstance GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void SetItemInstance(const FPRItemInstance& InItemInstance);

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool CanBePickedUp() const;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsPickedUp() const { return bIsPickedUp; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void SetPickedUp(bool bNewPickedUp);

	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

private:
	UFUNCTION()
	void OnRep_ItemInstance();

	UFUNCTION()
	void OnRep_IsPickedUp();

	void RefreshPickupVisualState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(ReplicatedUsing = OnRep_ItemInstance, EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	FPRItemInstance ItemInstance;

	UPROPERTY(ReplicatedUsing = OnRep_IsPickedUp, VisibleInstanceOnly, BlueprintReadOnly, Category = "Pickup", meta = (AllowPrivateAccess = "true"))
	bool bIsPickedUp;
};
