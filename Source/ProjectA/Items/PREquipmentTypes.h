#pragma once

#include "CoreMinimal.h"
#include "PREquipmentTypes.generated.h"

/** The fixed equipment roles introduced by the v0.7.1 runtime. */
UENUM(BlueprintType)
enum class EPREquipmentSlot : uint8
{
	None,
	Weapon,
	Armor,
	Chip,
	Tool
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPREquipmentAppearanceEntry
{
	GENERATED_BODY()

	/** Public-safe slot identity; no instance GUID, level, durability, or affixes are replicated here. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment|Appearance")
	FName SlotId = NAME_None;

	/** Public-safe item definition used by remote clients to resolve a cosmetic only. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment|Appearance")
	FName ItemId = NAME_None;

	bool IsValid() const { return !SlotId.IsNone() && !ItemId.IsNone(); }
};

namespace ProjectRiftEquipmentSlots
{
	inline const FName Weapon(TEXT("Slot.Primary"));
	inline const FName Armor(TEXT("Slot.Armor"));
	inline const FName Chip(TEXT("Slot.Chip"));
	inline const FName Tool(TEXT("Slot.Tool"));
}
