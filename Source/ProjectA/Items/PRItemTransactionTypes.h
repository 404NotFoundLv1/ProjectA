#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRItemTransactionTypes.generated.h"

UENUM(BlueprintType)
enum class EPRItemTransactionIntent : uint8
{
	None,
	Pickup,
	Use,
	Drop,
	EquipPrimary,
	UnequipPrimary,
	BeginReload,
	Equip,
	Unequip,
	BeginUse
};

UENUM(BlueprintType)
enum class EPRItemTransactionStatus : uint8
{
	Pending,
	Success,
	RevisionConflict,
	InvalidRequest,
	Unauthorized,
	NotFound,
	InvalidState,
	InsufficientQuantity,
	CapacityExceeded,
	WorldActionFailed,
	Cancelled,
	InternalFailure
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemTransactionHeader
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	FGuid TransactionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	int32 ExpectedRevision = 0;

	bool IsValid() const { return TransactionId.IsValid() && ExpectedRevision >= 0; }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemTransactionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	FPRItemTransactionHeader Header;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	EPRItemTransactionIntent Intent = EPRItemTransactionIntent::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	FGuid InstanceGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction", meta = (ClampMin = "0"))
	int32 Count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	TObjectPtr<AActor> TargetActor;

	/** Optional inventory grant committed atomically with a pending item use (for ammunition packs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction")
	FName GrantedItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Transaction", meta = (ClampMin = "0"))
	int32 GrantedItemCount = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRItemTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	EPRItemTransactionIntent Intent = EPRItemTransactionIntent::None;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	EPRItemTransactionStatus Status = EPRItemTransactionStatus::InvalidRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	int32 PreviousRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	int32 CurrentRevision = 0;

	/** The authoritative quantity actually moved by this completed transaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	int32 AppliedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	bool bWasReplay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	TArray<FGuid> ChangedInstanceGuids;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	TArray<FGuid> CreatedInstanceGuids;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	TArray<FGuid> RemovedInstanceGuids;

	UPROPERTY(BlueprintReadOnly, Category = "Item Transaction")
	FString Diagnostic;
};
