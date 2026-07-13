#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileSave.h"
#include "Persistence/PRSaveStorage.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Ship/PRShipRepairTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PRSaveSubsystem.generated.h"

class APlayerController;
class APRPlayerState;

UCLASS()
class PROJECTA_API UPRSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult CreateProfile(const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	TArray<FPRProfileSummary> GetProfiles();

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult ActivateAndLoadProfile(FGuid ProfileId);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult DeleteProfile(FGuid ProfileId);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult SaveActiveProfile(EPRSaveReason Reason = EPRSaveReason::Manual);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	bool GetActiveProfileSnapshot(UPARAM(ref) FPRProfileSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult ReplaceActiveProfileSnapshot(const FPRProfileSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult ApplyActiveProfileToPlayerState(APRPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Profile")
	FPRProfileOperationResult CaptureAndSaveAtSafeCheckpoint(APRPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Profile|Multiplayer")
	FPRProfileOperationResult BuildMultiplayerProfileProjection(UPARAM(ref) FPRMultiplayerProfileProjection& OutProjection) const;

	UFUNCTION(BlueprintCallable, Category = "Profile|Multiplayer")
	FPRProfileOperationResult BindActiveProfileToSession();

	UFUNCTION(BlueprintCallable, Category = "Profile|Multiplayer")
	void ReleaseSessionProfileBinding();

	UFUNCTION(BlueprintPure, Category = "Profile|Multiplayer")
	bool IsSessionProfileBound() const { return SessionBoundProfileId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Profile|Multiplayer")
	FGuid GetSessionBoundProfileId() const { return SessionBoundProfileId; }

	UFUNCTION(BlueprintCallable, Category = "Profile|Multiplayer")
	FPRProfileOperationResult ApplyMultiplayerSettlementReceipt(const FPRPlayerSettlementReceipt& Receipt);

	UFUNCTION(BlueprintPure, Category = "Profile|Multiplayer")
	bool HasPendingSettlementReceipt() const { return bHasPendingSettlementReceipt; }

	UFUNCTION(BlueprintPure, Category = "Profile|Multiplayer")
	FPRPlayerSettlementReceipt GetPendingSettlementReceipt() const { return PendingSettlementReceipt; }

	FPRProfileOperationResult RetryPendingSettlementReceipt();

	UFUNCTION(BlueprintPure, Category = "Profile|Ship Repair")
	FPRShipRepairEvaluation EvaluateShipRepair(FName RepairProjectId) const;

	UFUNCTION(BlueprintCallable, Category = "Profile|Ship Repair")
	FPRProfileOperationResult ApplyShipRepairReceipt(const FPRShipRepairReceipt& Receipt);

	UFUNCTION(BlueprintPure, Category = "Profile|Ship Repair")
	bool HasPendingShipRepairReceipt() const { return bHasPendingShipRepairReceipt; }

	UFUNCTION(BlueprintPure, Category = "Profile|Ship Repair")
	FPRShipRepairReceipt GetPendingShipRepairReceipt() const { return PendingShipRepairReceipt; }

	UFUNCTION(BlueprintCallable, Category = "Profile|Ship Repair")
	FPRProfileOperationResult RetryPendingShipRepairReceipt();

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult PrepareShipRepairAcceptanceForDevelopment();

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult CreateLegacyV1ProfileForDevelopment();

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult CorruptActivePrimaryForDevelopment();

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult FailNextSaveForDevelopment();

	UFUNCTION(BlueprintPure, Category = "Profile|Development")
	bool IsUsingIsolatedDevelopmentRoot() const { return bUsingIsolatedDevelopmentRoot; }

	UFUNCTION(BlueprintPure, Category = "Profile|Development")
	bool IsGauntletSmokeInitializationRejected() const { return bGauntletSmokeInitializationRejected; }

	static bool ResolveGauntletSmokeProfileRoot(
		const FString& RunIdText,
		const FString& CustomUserDirectory,
		const FString& ProjectAutomationDirectory,
		FString& OutProfileRoot,
		FString& OutDiagnostic);

	UFUNCTION(BlueprintPure, Category = "Profile")
	FGuid GetActiveProfileId() const { return ActiveProfile ? ActiveProfile->ProfileId : FGuid(); }

	UFUNCTION(BlueprintPure, Category = "Profile")
	FString GetActiveProfileDisplayName() const { return ActiveProfile ? ActiveProfile->DisplayName : FString(); }

	UFUNCTION(BlueprintPure, Category = "Profile")
	FPRProfileOperationResult GetLastOperationResult() const { return LastOperationResult; }

	void HandleLocalLobbyPlayerReady(APlayerController* PlayerController);
	FPRProfileOperationResult SaveForApplicationExit();

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(const FString& RootDirectory);
#endif

private:
	void LoadOrRebuildCatalog();
	FPRProfileOperationResult ReconcileCatalogWithDisk();
	bool IsUnsafeRiftCaptureContext(const APRPlayerState* PlayerState) const;
	void SetLastResult(const FPRProfileOperationResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<UPRProfileCatalogSave> Catalog;

	UPROPERTY(Transient)
	TObjectPtr<UPRProfileSave> ActiveProfile;

	TUniquePtr<FPRSafeSaveStore> SaveStore;
	FPRProfileOperationResult LastOperationResult;
	bool bHasAppliedActiveProfile = false;
	FGuid SessionBoundProfileId;
	FPRPlayerSettlementReceipt PendingSettlementReceipt;
	bool bHasPendingSettlementReceipt = false;
	FPRShipRepairReceipt PendingShipRepairReceipt;
	bool bHasPendingShipRepairReceipt = false;
	bool bUsingIsolatedDevelopmentRoot = false;
	bool bGauntletSmokeInitializationRejected = false;
};
