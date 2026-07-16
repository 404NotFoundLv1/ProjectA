#pragma once

#include "CoreMinimal.h"
#include "PRDeployableTypes.generated.h"

UENUM(BlueprintType)
enum class EPRDeployableKind : uint8
{
	None UMETA(Hidden),
	Sentry,
	RepairDrone,
	ShieldGenerator
};

UENUM(BlueprintType)
enum class EPRDeployablePlacementResult : uint8
{
	Started,
	Confirmed,
	Cancelled,
	InvalidState,
	InvalidModule,
	InvalidTransform,
	OutOfRange,
	Obstructed,
	InvalidSurface,
	Occupied,
	Expired,
	Duplicate,
	InternalFailure
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDeployablePlacementState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deployable")
	FName ModuleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Deployable")
	EPRDeployableKind DeployableKind = EPRDeployableKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Deployable")
	int32 SessionSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deployable")
	float ExpiresAtServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Deployable")
	bool bPending = false;
};
