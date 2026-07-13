#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileSave.h"
#include "Persistence/PRSaveStorage.h"
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

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult CreateLegacyV1ProfileForDevelopment();

	UFUNCTION(BlueprintCallable, Category = "Profile|Development")
	FPRProfileOperationResult CorruptActivePrimaryForDevelopment();

	UFUNCTION(BlueprintPure, Category = "Profile")
	FGuid GetActiveProfileId() const { return ActiveProfile ? ActiveProfile->ProfileId : FGuid(); }

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
};
