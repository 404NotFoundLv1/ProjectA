#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileTypes.h"
#include "PRShipRepairTypes.generated.h"

UENUM(BlueprintType)
enum class EPRShipRepairAvailability : uint8
{
	Available,
	AlreadyCompleted,
	InvalidContract,
	MissingPrerequisiteRepair,
	MissingStoryProgress,
	InsufficientResources
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRShipRepairResourceCost
{
	GENERATED_BODY()

	FPRShipRepairResourceCost() = default;
	FPRShipRepairResourceCost(const FName InResourceId, const int32 InCount)
		: ResourceId(InResourceId), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Repair", meta = (ClampMin = "1"))
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship Repair", meta = (ClampMin = "1"))
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRShipRepairEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	EPRShipRepairAvailability Availability = EPRShipRepairAvailability::InvalidContract;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	FString Diagnostic;

	bool IsAvailable() const { return Availability == EPRShipRepairAvailability::Available; }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRShipRepairReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	FName RepairProjectId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	TArray<FPRProfileResourceBalance> SettledResourceWallet;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	TArray<FPRProfileShipModuleState> SettledShipModules;

	UPROPERTY(BlueprintReadOnly, Category = "Ship Repair")
	FPRProfileStoryProgress SettledStory;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};
