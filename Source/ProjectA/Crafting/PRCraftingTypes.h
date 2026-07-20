#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileTypes.h"
#include "PRCraftingTypes.generated.h"

UENUM(BlueprintType)
enum class EPRCraftingAvailability : uint8
{
	Available,
	InvalidRecipe,
	MissingStoryProgress,
	MissingChapter,
	MissingShipModule,
	InsufficientResources,
	CapacityExceeded,
	InvalidTarget,
	AlreadyMaxLevel
};

UENUM(BlueprintType)
enum class EPRCraftingOperation : uint8
{
	Craft,
	Dismantle,
	Upgrade
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRCraftingResourceCost
{
	GENERATED_BODY()

	FPRCraftingResourceCost() = default;
	FPRCraftingResourceCost(const FName InResourceId, const int32 InCount)
		: ResourceId(InResourceId), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1"))
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1"))
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDismantleResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TArray<FPRCraftingResourceCost> ResourceReturns;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRUpgradeCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "2"))
	int32 TargetLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TArray<FPRCraftingResourceCost> ResourceCosts;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRUpgradeAttributeModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	EPRAffixAttribute Attribute = EPRAffixAttribute::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "0.0001"))
	float MagnitudePerLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRCraftingEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	EPRCraftingAvailability Availability = EPRCraftingAvailability::InvalidRecipe;

	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	FString Diagnostic;

	bool IsAvailable() const { return Availability == EPRCraftingAvailability::Available; }
};

/** Absolute server-authored state sent to the profile owner and persisted with replay protection. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRCraftingReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FGuid ProfileId;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FGuid TransactionId;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") EPRCraftingOperation Operation = EPRCraftingOperation::Craft;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FName RecipeId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") FGuid TargetInstanceGuid;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") int32 CraftSeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") TArray<FPRProfileResourceBalance> SettledResourceWallet;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") TArray<FPRItemInstance> SettledBackpackItems;
	UPROPERTY(BlueprintReadOnly, Category = "Crafting") TArray<FPRProfileEquipmentEntry> SettledEquipment;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};
