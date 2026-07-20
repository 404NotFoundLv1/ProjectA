#pragma once

#include "CoreMinimal.h"
#include "Core/PRRiftSettlementTypes.h"
#include "Persistence/PRProfileTypes.h"
#include "PRMultiplayerProfileTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FPRMultiplayerProfileProjection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRItemInstance> BackpackItems;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRQuickSlotReference> QuickSlots;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRProfileEquipmentEntry> Equipment;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRProfileResourceBalance> ResourceWallet;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	FName SelectedRoleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FName> UnlockedRoleIds;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FName> UnlockedRoleModuleIds;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRRoleModuleSlotEntry> EquippedRoleModules;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	FPRProfileStoryProgress Story;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRProfileShipModuleState> ShipModules;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Profile")
	TArray<FPRLootProtectionState> LootProtectionStates;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRPlayerSettlementReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	int32 RunSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FGuid SettlementId;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	EPRRiftMissionResult Result = EPRRiftMissionResult::None;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	bool bExtracted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	bool bGrantStoryProgress = false;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRItemInstance> SettledBackpackItems;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRQuickSlotReference> SettledQuickSlots;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRProfileEquipmentEntry> SettledEquipment;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRProfileResourceBalance> SettledResourceWallet;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FName SettledRoleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FName> SettledUnlockedRoleIds;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FName> SettledUnlockedRoleModuleIds;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRRoleModuleSlotEntry> SettledEquippedRoleModules;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRItemInstance> GrantedWarehouseItems;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	FPRLootProtectionState UpdatedLootProtectionState;

	UPROPERTY(BlueprintReadOnly, Category = "Multiplayer|Settlement")
	TArray<FPRRewardAuditEntry> RewardAuditEntries;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

class PROJECTA_API FPRMultiplayerSettlementPolicy
{
public:
	static TArray<FPRItemInstance> BuildNonExtractedInventory(
		const TArray<FPRItemInstance>& BaselineItems,
		const TArray<FPRItemInstance>& RuntimeItems);
	static TArray<FPRProfileResourceBalance> BuildNonExtractedResourceWallet(
		const TArray<FPRProfileResourceBalance>& BaselineWallet,
		const TArray<FPRProfileResourceBalance>& RuntimeWallet);
	static int32 GetItemCount(const TArray<FPRItemInstance>& Items, FName ItemId);
};
