#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRCraftingStation.generated.h"

/** Authoritative ship-lobby interaction point for crafting operations. */
UCLASS()
class PROJECTA_API APRCraftingStation : public AActor
{
	GENERATED_BODY()

public:
	APRCraftingStation();

	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetInteractionRadius() const { return InteractionRadius; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	FText GetInteractionPromptText() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USphereComponent> InteractionSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true", ClampMin = "50.0"))
	float InteractionRadius = 220.0f;
};
