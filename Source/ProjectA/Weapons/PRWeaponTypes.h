#pragma once

#include "CoreMinimal.h"
#include "PRWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class EPRWeaponFireResult : uint8
{
	FiredMiss,
	FiredHit,
	NoWeapon,
	Empty,
	Reloading,
	RateLimited,
	Inactive,
	Invalid
};

namespace ProjectRiftWeaponSlots
{
	inline const FName Primary(TEXT("Slot.Primary"));
}
