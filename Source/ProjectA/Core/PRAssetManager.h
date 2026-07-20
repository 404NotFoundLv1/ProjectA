#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "PRAssetManager.generated.h"

class UPRItemDataAsset;
class UPRAffixDefinitionDataAsset;
class UPRLootTableDataAsset;
class UPRMissionProgressionDataAsset;
class UPRRoleDataAsset;
class UPRRoleModuleDataAsset;
class UPRShipRepairDataAsset;
struct FStreamableHandle;

DECLARE_DELEGATE_OneParam(FPRItemDataLoadComplete, UPRItemDataAsset*);
DECLARE_DELEGATE_OneParam(FPRLootTableLoadComplete, UPRLootTableDataAsset*);

UCLASS()
class PROJECTA_API UPRAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static UPRAssetManager* Get();
	static FPrimaryAssetId MakeItemPrimaryAssetId(FName ItemId);
	static FPrimaryAssetId MakeAffixPrimaryAssetId(FName AffixId);
	static FPrimaryAssetId MakeLootTablePrimaryAssetId(FName AssetName);
	static FPrimaryAssetId MakeMissionPrimaryAssetId(FName MissionId);
	static FPrimaryAssetId MakeRolePrimaryAssetId(FName RoleId);
	static FPrimaryAssetId MakeRoleModulePrimaryAssetId(FName ModuleId);
	static FPrimaryAssetId MakeShipRepairPrimaryAssetId(FName RepairProjectId);

	virtual void StartInitialLoading() override;

	UPRItemDataAsset* GetLoadedItemData(FName ItemId) const;
	UPRLootTableDataAsset* GetLoadedLootTable(FName AssetName) const;
	UPRItemDataAsset* LoadItemDataSync(FName ItemId);
	UPRAffixDefinitionDataAsset* LoadAffixSync(FName AffixId);
	bool LoadAffixCatalog(TArray<UPRAffixDefinitionDataAsset*>& OutAffixes);
	UPRLootTableDataAsset* LoadLootTableSync(FName AssetName);
	UPRMissionProgressionDataAsset* LoadMissionSync(FName MissionId);
	UPRRoleDataAsset* LoadRoleSync(FName RoleId);
	UPRRoleModuleDataAsset* LoadRoleModuleSync(FName ModuleId);
	bool LoadRoleCatalog(TArray<UPRRoleDataAsset*>& OutRoles, TArray<UPRRoleModuleDataAsset*>& OutModules);
	UPRShipRepairDataAsset* LoadShipRepairSync(FName RepairProjectId);
	bool LoadShipRepairCatalog(TArray<UPRShipRepairDataAsset*>& OutCatalog);
	TSharedPtr<FStreamableHandle> LoadItemDataAsync(FName ItemId, FPRItemDataLoadComplete Completion);
	TSharedPtr<FStreamableHandle> LoadLootTableAsync(FName AssetName, FPRLootTableLoadComplete Completion);

private:
	UObject* LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass);
	void ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const;
};
