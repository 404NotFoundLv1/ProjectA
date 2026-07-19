#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/PRItemTransactionTypes.h"
#include "PRItemTransactionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPRItemTransactionResultDelegate, const FPRItemTransactionResult&, Result);

UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRItemTransactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRItemTransactionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Item Transaction")
	FPRItemTransactionResultDelegate OnTransactionResult;

	UFUNCTION(BlueprintPure, Category = "Item Transaction")
	int32 GetRevision() const { return Revision; }

	/** Server-trusted execution entry used by gameplay systems after they build a semantic request. */
	FPRItemTransactionResult ExecuteAuthoritativeTransaction(const FPRItemTransactionRequest& Request);

	/** Finalizes a previously accepted reload using the reserve ammunition available at completion time. */
	FPRItemTransactionResult CompletePendingReload(const FGuid& TransactionId, FName AmmoItemId, int32 RequestedCount);

	/** Cancels a pending reload without mutating inventory or advancing the revision. */
	FPRItemTransactionResult CancelPendingReload(const FGuid& TransactionId, const FString& Diagnostic);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Item Transaction")
	void ServerSubmitItemTransaction(const FPRItemTransactionRequest& Request);

	UFUNCTION(Client, Reliable, Category = "Item Transaction")
	void ClientReceiveItemTransactionResult(const FPRItemTransactionResult& Result);

	void CopyRuntimeStateFrom(const UPRItemTransactionComponent* SourceComponent);
	void ResetForNewProfileBinding();

private:
	FPRItemTransactionResult ResolveRequest(const FPRItemTransactionRequest& Request);
	void CacheFinalResult(const FPRItemTransactionResult& Result);
	const FPRItemTransactionResult* FindCachedResult(const FGuid& TransactionId) const;
	const FPRItemTransactionResult* FindPendingResult(const FGuid& TransactionId) const;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Item Transaction", meta = (AllowPrivateAccess = "true"))
	int32 Revision = 0;

	UPROPERTY(Transient)
	TArray<FPRItemTransactionResult> RecentResults;

	UPROPERTY(Transient)
	TArray<FPRItemTransactionResult> PendingResults;

	static constexpr int32 MaxCachedResults = 256;
};
