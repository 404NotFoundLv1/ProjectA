#pragma once

#include "CoreMinimal.h"
#include "Items/PRItemTypes.h"
#include "PRProfileTypes.generated.h"

UENUM(BlueprintType)
enum class EPRProfileOperationStatus : uint8
{
	Success,
	NoActiveProfile,
	NotFound,
	Corrupt,
	UnsupportedFutureVersion,
	MigrationFailed,
	ValidationFailed,
	UnsafeCaptureContext,
	Busy,
	IOError
};

UENUM(BlueprintType)
enum class EPRSaveReason : uint8
{
	Manual,
	SafeCheckpoint,
	ApplicationExit,
	Migration
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	EPRProfileOperationStatus Status = EPRProfileOperationStatus::Success;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 LoadedVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	bool bRecoveredFromBackup = false;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	bool bMigrated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FString Diagnostic;

	bool IsSuccess() const { return Status == EPRProfileOperationStatus::Success; }

	static FPRProfileOperationResult MakeSuccess(const FGuid& InProfileId = FGuid());
	static FPRProfileOperationResult MakeFailure(EPRProfileOperationStatus InStatus, const FString& InDiagnostic, const FGuid& InProfileId = FGuid());
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileResourceBalance
{
	GENERATED_BODY()

	FPRProfileResourceBalance() = default;
	FPRProfileResourceBalance(const FName InResourceId, const int32 InCount)
		: ResourceId(InResourceId), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Resources")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Resources", meta = (ClampMin = "0"))
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileEquipmentEntry
{
	GENERATED_BODY()

	FPRProfileEquipmentEntry() = default;
	FPRProfileEquipmentEntry(const FName InSlotId, const FPRItemInstance& InItem)
		: SlotId(InSlotId), Item(InItem) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Equipment")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Equipment")
	FPRItemInstance Item;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileShipModuleState
{
	GENERATED_BODY()

	FPRProfileShipModuleState() = default;
	FPRProfileShipModuleState(const FName InModuleId, const int32 InLevel, const bool bInUnlocked)
		: ModuleId(InModuleId), Level(InLevel), bUnlocked(bInUnlocked) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Ship")
	FName ModuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Ship", meta = (ClampMin = "0"))
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Ship")
	bool bUnlocked = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileSettingsData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings")
	FString CultureName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EffectsVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float MouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings")
	bool bSubtitlesEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings")
	bool bReduceCameraShake = false;

	void Normalize();
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileStoryProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Story")
	FName CurrentChapterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Story")
	TArray<FName> UnlockedChapterIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Story")
	TArray<FName> CompletedStoryNodeIds;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Resources")
	TArray<FPRProfileResourceBalance> ResourceWallet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Inventory")
	TArray<FPRItemInstance> BackpackItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Inventory")
	TArray<FPRItemInstance> WarehouseItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Equipment")
	TArray<FPRProfileEquipmentEntry> Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Roles")
	TArray<FName> UnlockedRoleIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Roles")
	FName SelectedRoleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Ship")
	TArray<FPRProfileShipModuleState> ShipModules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Settings")
	FPRProfileSettingsData Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile|Story")
	FPRProfileStoryProgress Story;

	int32 GetResourceCount(FName ResourceId) const;
	void Normalize();
	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRProfileSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 SaveVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FDateTime CreatedUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	FDateTime LastSavedUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	bool bIsActive = false;
};
