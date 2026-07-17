#pragma once

#include "CoreMinimal.h"
#include "PRReviveTypes.generated.h"

UENUM(BlueprintType)
enum class EPRReviveSource : uint8
{
	None,
	Player,
	RescueDrone,
	System
};

UENUM(BlueprintType)
enum class EPRRescueDroneState : uint8
{
	Unavailable,
	Ready,
	Deployed,
	Spent
};
