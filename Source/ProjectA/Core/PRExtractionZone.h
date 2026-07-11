#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRExtractionZone.generated.h"

class APRRiftGameMode;
class APRRiftGameState;
class APawn;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * Server-authoritative extraction trigger for rift missions.
 */
UCLASS()
class PROJECTA_API APRExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	APRExtractionZone();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	bool CanExtractPawn(APawn* ExtractingPawn) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rift|Extraction")
	bool TryExtractPawn(APawn* ExtractingPawn);

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	float GetExtractionRadius() const;

	UFUNCTION(BlueprintPure, Category = "Rift|Extraction")
	FText GetInteractionPromptText() const;

	UWidgetComponent* GetInteractionPromptWidget() const { return InteractionPromptWidget; }

protected:
	void SetInteractionPromptVisible(bool bVisible);
	void RefreshInteractionPromptWidget();
	FLinearColor GetInteractionPromptColor() const;
	void UpdateInteractionPromptVisibility();
	APawn* FindNearbyPromptPawn() const;
	void TryExtractNearbyPawns();
	APRRiftGameMode* GetRiftGameMode() const;
	APRRiftGameState* GetRiftGameState() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Extraction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Extraction")
	TObjectPtr<UStaticMeshComponent> ZoneMarkerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Extraction")
	TObjectPtr<USphereComponent> ExtractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rift|Extraction")
	TObjectPtr<UWidgetComponent> InteractionPromptWidget;

	UFUNCTION()
	void HandleExtractionSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleExtractionSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
