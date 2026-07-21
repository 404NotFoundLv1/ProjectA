#pragma once

#include "CoreMinimal.h"
#include "PRMissionContractTypes.generated.h"

/**
 * A deliberately high-level reward promise.  It communicates the contract's
 * budget without prematurely creating inventory instances.
 */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionRewardPreview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FName> RewardTypeIds { FName(TEXT("Reward.Material")) };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0"))
	int32 MinimumBudget = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0"))
	int32 MaximumBudget = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0"))
	int32 MinimumRarity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0"))
	int32 MaximumRarity = 0;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

/** Authored, asset-owned inputs used to create a concrete mission definition. */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionContract
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "1"))
	int32 ContractVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName BiomeId = FName(TEXT("Biome.Test"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName DifficultyId = FName(TEXT("Difficulty.Standard"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FName> ModifierIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FPRMissionRewardPreview RewardPreview;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

/**
 * The server-authoritative, replicated description of the selected mission.
 * This remains POD data so it can cross travel without any UObject reference.
 */
USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionDefinition
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	FName ContractId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 ContractVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	FName BiomeId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	FName DifficultyId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FName> ModifierIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	FPRMissionRewardPreview RewardPreview;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
	int32 DeterministicSignature = 0;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

/** Compact URL payload for the existing lobby-to-rift seamless travel path. */
USTRUCT()
struct PROJECTA_API FPRMissionTravelContext
{
	GENERATED_BODY()

	UPROPERTY()
	FName ContractId = NAME_None;

	UPROPERTY()
	int32 ContractVersion = 0;

	UPROPERTY()
	int32 Seed = 0;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
	FString ToTravelOptions() const;
	static bool ParseOptions(const FString& Options, FPRMissionTravelContext& OutContext, FString* OutDiagnostic = nullptr);
	static bool ParseOptions(const FString& Options, FPRMissionTravelContext& OutContext, FString& OutDiagnostic)
	{
		return ParseOptions(Options, OutContext, &OutDiagnostic);
	}
};
