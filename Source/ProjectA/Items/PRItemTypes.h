#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PRItemTypes.generated.h"

UENUM(BlueprintType)
enum class EPRItemType : uint8
{
	Consumable,
	Equipment,
	Material,
	QuestItem,
	Ammunition
};

UENUM(BlueprintType)
enum class EPRItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary,
	/** v0.7.2 generation-only top rarity. Epic/Legendary remain load-compatible legacy values. */
	Prototype
};

UENUM(BlueprintType)
enum class EPRAffixAttribute : uint8
{
	MaxHealth,
	MaxShield,
	MaxEnergy,
	AttackPower,
	MoveSpeed,
	HealingPower,
	PollutionResistance
};

UENUM(BlueprintType)
enum class EPRAffixModifierType : uint8
{
	Additive,
	Percentage
};

/** Final, server-generated affix payload. Magnitude is the player-readable delta; percentages use 0.08 for +8%. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRAffixRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	FName AffixId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	EPRAffixAttribute Attribute = EPRAffixAttribute::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	EPRAffixModifierType ModifierType = EPRAffixModifierType::Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	float Magnitude = 0.0f;

	bool IsValid() const
	{
		return !AffixId.IsNone() && FMath::IsFinite(Magnitude) && Magnitude > 0.0f;
	}

	bool operator==(const FPRAffixRoll& Other) const
	{
		return AffixId == Other.AffixId
			&& Attribute == Other.Attribute
			&& ModifierType == Other.ModifierType
			&& Magnitude == Other.Magnitude;
	}
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemInstance
{
	GENERATED_BODY()

	/** Stable server-assigned identity for one concrete stack or equipped item. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Identity")
	FGuid InstanceGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EPRItemRarity Rarity = EPRItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Durability = 1.0f;

	/** Legacy ID-only representation. New generated items mirror RolledAffixes here for backwards-compatible UI and saves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<FName> Affixes;

	/** Server seed retained for reproducibility and diagnostics. Valid only when AffixGenerationVersion is non-zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	int32 LootSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	int32 AffixGenerationVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Affix")
	TArray<FPRAffixRoll> RolledAffixes;

	bool IsValid() const
	{
		return !ItemId.IsNone() && Count > 0;
	}

	bool HasValidIdentity() const
	{
		return InstanceGuid.IsValid();
	}

	bool HasEquivalentStackingState(const FPRItemInstance& Other) const
	{
		return ItemId == Other.ItemId
			&& Level == Other.Level
			&& Rarity == Other.Rarity
			&& FMath::IsNearlyEqual(Durability, Other.Durability)
			&& Affixes == Other.Affixes
			&& LootSeed == Other.LootSeed
			&& AffixGenerationVersion == Other.AffixGenerationVersion
			&& RolledAffixes == Other.RolledAffixes;
	}
};
