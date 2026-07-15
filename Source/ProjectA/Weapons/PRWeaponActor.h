#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRWeaponActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Replicated placeholder firearm assembled from engine primitive meshes. */
UCLASS()
class PROJECTA_API APRWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	APRWeaponActor();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FTransform GetMuzzleTransform() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> ReceiverMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> GripMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<USceneComponent> Muzzle;
};
