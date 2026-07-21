#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "PRAssetManager.generated.h"

class UPRItemDataAsset;
class UPRAffixDefinitionDataAsset;
class UPRLootTableDataAsset;
class UPRRewardBudgetDataAsset;
class UPRMissionProgressionDataAsset;
class UPRMissionModifierDefinitionDataAsset;
class UPRRoleDataAsset;
class UPRRoleModuleDataAsset;
class UPRShipRepairDataAsset;
class UPRCraftingRecipeDataAsset;
class UPREnemyDefinitionDataAsset;
class UPREnemyRosterDataAsset;
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
	static FPrimaryAssetId MakeRewardBudgetPrimaryAssetId(FName AssetName);
	static FPrimaryAssetId MakeMissionPrimaryAssetId(FName MissionId);
	static FPrimaryAssetId MakeMissionModifierPrimaryAssetId(FName ModifierId);
	static FPrimaryAssetId MakeRolePrimaryAssetId(FName RoleId);
	static FPrimaryAssetId MakeRoleModulePrimaryAssetId(FName ModuleId);
	static FPrimaryAssetId MakeShipRepairPrimaryAssetId(FName RepairProjectId);
	static FPrimaryAssetId MakeCraftingRecipePrimaryAssetId(FName RecipeId);
	static FPrimaryAssetId MakeEnemyPrimaryAssetId(FName EnemyId);
	static FPrimaryAssetId MakeEnemyRosterPrimaryAssetId(FName RosterId);

	virtual void StartInitialLoading() override;

	UPRItemDataAsset* GetLoadedItemData(FName ItemId) const;
	UPRLootTableDataAsset* GetLoadedLootTable(FName AssetName) const;
	UPRItemDataAsset* LoadItemDataSync(FName ItemId);
	UPRAffixDefinitionDataAsset* LoadAffixSync(FName AffixId);
	bool LoadAffixCatalog(TArray<UPRAffixDefinitionDataAsset*>& OutAffixes);
	UPRLootTableDataAsset* LoadLootTableSync(FName AssetName);
	UPRRewardBudgetDataAsset* LoadRewardBudgetSync(FName AssetName);
	UPRMissionProgressionDataAsset* LoadMissionSync(FName MissionId);
	UPRMissionModifierDefinitionDataAsset* LoadMissionModifierSync(FName ModifierId);
	bool LoadMissionCatalog(TArray<UPRMissionProgressionDataAsset*>& OutCatalog);
	UPRRoleDataAsset* LoadRoleSync(FName RoleId);
	UPRRoleModuleDataAsset* LoadRoleModuleSync(FName ModuleId);
	bool LoadRoleCatalog(TArray<UPRRoleDataAsset*>& OutRoles, TArray<UPRRoleModuleDataAsset*>& OutModules);
	UPRShipRepairDataAsset* LoadShipRepairSync(FName RepairProjectId);
	bool LoadShipRepairCatalog(TArray<UPRShipRepairDataAsset*>& OutCatalog);
	UPRCraftingRecipeDataAsset* LoadCraftingRecipeSync(FName RecipeId);
	bool LoadCraftingRecipeCatalog(TArray<UPRCraftingRecipeDataAsset*>& OutCatalog);
	UPREnemyDefinitionDataAsset* LoadEnemyDefinitionSync(FName EnemyId);
	UPREnemyRosterDataAsset* LoadEnemyRosterSync(FName RosterId);
	bool LoadEnemyCatalog(TArray<UPREnemyDefinitionDataAsset*>& OutDefinitions, TArray<UPREnemyRosterDataAsset*>& OutRosters);
	TSharedPtr<FStreamableHandle> LoadItemDataAsync(FName ItemId, FPRItemDataLoadComplete Completion);
	TSharedPtr<FStreamableHandle> LoadLootTableAsync(FName AssetName, FPRLootTableLoadComplete Completion);

private:
	void RefreshPrimaryAssetCatalogs();
	UObject* LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass);
	void ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const;
};
