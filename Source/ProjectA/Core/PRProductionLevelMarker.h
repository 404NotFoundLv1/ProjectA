#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRProductionLevelMarker.generated.h"

class USceneComponent;

/**
 * Stable, data-only marker used by production-map validation. It has no gameplay behavior.
 */
UENUM(BlueprintType)
enum class EPRProductionLevelMarkerKind : uint8
{
	StreamingPartition
};

UCLASS()
class PROJECTA_API APRProductionLevelMarker : public AActor
{
	GENERATED_BODY()

public:
	APRProductionLevelMarker();

	UFUNCTION(BlueprintPure, Category = "ProjectRift|Production")
	FName GetMarkerId() const { return MarkerId; }

	UFUNCTION(BlueprintPure, Category = "ProjectRift|Production")
	EPRProductionLevelMarkerKind GetMarkerKind() const { return MarkerKind; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProjectRift|Production", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ProjectRift|Production", meta = (AllowPrivateAccess = "true"))
	FName MarkerId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ProjectRift|Production", meta = (AllowPrivateAccess = "true"))
	EPRProductionLevelMarkerKind MarkerKind = EPRProductionLevelMarkerKind::StreamingPartition;
};
