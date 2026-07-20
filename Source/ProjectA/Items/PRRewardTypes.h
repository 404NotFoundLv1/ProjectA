#pragma once

#include "CoreMinimal.h"
#include "Items/PRItemTypes.h"
#include "PRRewardTypes.generated.h"

UENUM(BlueprintType)
enum class EPRLootDistributionPolicy : uint8
{
	SharedWorld,
	PersonalSettlement
};

UENUM(BlueprintType)
enum class EPRRewardSourceType : uint8
{
	Enemy,
	MissionSettlement,
	Objective
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRewardSourceContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	EPRRewardSourceType SourceType = EPRRewardSourceType::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	FGuid RecipientProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	int32 Ordinal = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRewardBudgetSourceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	EPRRewardSourceType SourceType = EPRRewardSourceType::MissionSettlement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	EPRLootDistributionPolicy DistributionPolicy = EPRLootDistributionPolicy::PersonalSettlement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards", meta = (ClampMin = "0"))
	int32 BudgetValue = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRLootProtectionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	FName RewardBudgetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ConsecutiveBelowRare = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards")
	FName LastItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ConsecutiveSameItem = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRRewardAuditEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	FPRRewardSourceContext Source;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	FName RewardBudgetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	int32 BudgetSpent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	FPRItemInstance GrantedItem;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRPersonalRewardGenerationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	int32 TotalBudget = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	int32 RemainingBudget = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	TArray<FPRItemInstance> GrantedWarehouseItems;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	int32 CommonChipCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	FPRLootProtectionState UpdatedProtectionState;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	TArray<FPRRewardAuditEntry> AuditEntries;

	UPROPERTY(BlueprintReadOnly, Category = "Rewards")
	FString Diagnostic;
};
