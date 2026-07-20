#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/PRItemTypes.h"
#include "PRAffixGenerationLibrary.generated.h"

class UPRAffixDefinitionDataAsset;
class UPREquipmentDataAsset;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRarityGenerationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	EPRItemRarity Rarity = EPRItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0001"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0", ClampMax = "3"))
	int32 AffixCount = 0;

	bool IsValid() const { return FMath::IsFinite(Weight) && Weight > 0.0f && AffixCount >= 0 && AffixCount <= 3; }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRAffixGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	FPRItemInstance Item;

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	FString Diagnostic;
};

/** Deterministic, server-owned equipment rarity and affix generation. */
UCLASS()
class PROJECTA_API UPRAffixGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentGenerationVersion = 1;

	static TArray<FPRRarityGenerationRule> MakeDefaultRarityRules();
	static bool ValidateRarityRules(const TArray<FPRRarityGenerationRule>& Rules, FString& OutDiagnostic);

	UFUNCTION(BlueprintCallable, Category = "Loot|Affix")
	static FPRAffixGenerationResult GenerateEquipmentInstance(
		const UPREquipmentDataAsset* EquipmentDefinition,
		int32 LootSeed,
		const TArray<UPRAffixDefinitionDataAsset*>& Definitions,
		const TArray<FPRRarityGenerationRule>& RarityRules);
};

