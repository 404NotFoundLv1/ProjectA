#include "Core/PRAssetManager.h"

#include "Items/PRItemDataAsset.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRRewardBudgetDataAsset.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Ship/PRShipRepairDataAsset.h"
#include "Crafting/PRCraftingRecipeDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StreamableManager.h"
#include "Modules/ModuleManager.h"
#include "ProjectA.h"

UPRAssetManager* UPRAssetManager::Get()
{
	if (!UAssetManager::IsInitialized())
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift AssetManager is not initialized."));
		return nullptr;
	}

	UPRAssetManager* Manager = Cast<UPRAssetManager>(&UAssetManager::Get());
	if (!Manager)
	{
		UE_LOG(
			LogProjectA,
			Error,
			TEXT("ProjectRift requires UPRAssetManager but the active manager is %s."),
			*GetNameSafe(&UAssetManager::Get()));
	}

	return Manager;
}

FPrimaryAssetId UPRAssetManager::MakeItemPrimaryAssetId(const FName ItemId)
{
	return ItemId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRItemDataAsset::ItemPrimaryAssetType, ItemId);
}

FPrimaryAssetId UPRAssetManager::MakeAffixPrimaryAssetId(const FName AffixId)
{
	return AffixId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRAffixDefinitionDataAsset::AffixPrimaryAssetType, AffixId);
}

FPrimaryAssetId UPRAssetManager::MakeLootTablePrimaryAssetId(const FName AssetName)
{
	return AssetName.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRLootTableDataAsset::LootTablePrimaryAssetType, AssetName);
}

FPrimaryAssetId UPRAssetManager::MakeRewardBudgetPrimaryAssetId(const FName AssetName)
{
	return AssetName.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRRewardBudgetDataAsset::RewardBudgetPrimaryAssetType, AssetName);
}

FPrimaryAssetId UPRAssetManager::MakeMissionPrimaryAssetId(const FName MissionId)
{
	return MissionId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRMissionProgressionDataAsset::MissionPrimaryAssetType, MissionId);
}

FPrimaryAssetId UPRAssetManager::MakeRolePrimaryAssetId(const FName RoleId)
{
	return RoleId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRRoleDataAsset::RolePrimaryAssetType, RoleId);
}

FPrimaryAssetId UPRAssetManager::MakeRoleModulePrimaryAssetId(const FName ModuleId)
{
	return ModuleId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRRoleModuleDataAsset::RoleModulePrimaryAssetType, ModuleId);
}

FPrimaryAssetId UPRAssetManager::MakeShipRepairPrimaryAssetId(const FName RepairProjectId)
{
	return RepairProjectId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRShipRepairDataAsset::ShipRepairPrimaryAssetType, RepairProjectId);
}

FPrimaryAssetId UPRAssetManager::MakeCraftingRecipePrimaryAssetId(const FName RecipeId)
{
	return RecipeId.IsNone()
		? FPrimaryAssetId()
		: FPrimaryAssetId(UPRCraftingRecipeDataAsset::CraftingRecipePrimaryAssetType, RecipeId);
}

void UPRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// The asset manager starts before the editor's first registry scan has
	// completed.  Registering primary-asset paths only here leaves every
	// ProjectRift catalog empty in editor automation and in a cold launch.
	// Refresh once the registry is authoritative instead.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UPRAssetManager::RefreshPrimaryAssetCatalogs);
	}
	else
	{
		RefreshPrimaryAssetCatalogs();
	}
}

void UPRAssetManager::RefreshPrimaryAssetCatalogs()
{
	ScanPathsForPrimaryAssets(UPRItemDataAsset::ItemPrimaryAssetType, { TEXT("/Game/ProjectRift/Items") }, UPRItemDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRAffixDefinitionDataAsset::AffixPrimaryAssetType, { TEXT("/Game/ProjectRift/Affixes") }, UPRAffixDefinitionDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRLootTableDataAsset::LootTablePrimaryAssetType, { TEXT("/Game/ProjectRift/Items"), TEXT("/Game/ProjectRift/Rewards") }, UPRLootTableDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRRewardBudgetDataAsset::RewardBudgetPrimaryAssetType, { TEXT("/Game/ProjectRift/Rewards") }, UPRRewardBudgetDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRMissionProgressionDataAsset::MissionPrimaryAssetType, { TEXT("/Game/ProjectRift/Missions") }, UPRMissionProgressionDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRRoleDataAsset::RolePrimaryAssetType, { TEXT("/Game/ProjectRift/Roles") }, UPRRoleDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRRoleModuleDataAsset::RoleModulePrimaryAssetType, { TEXT("/Game/ProjectRift/RoleModules") }, UPRRoleModuleDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRShipRepairDataAsset::ShipRepairPrimaryAssetType, { TEXT("/Game/ProjectRift/ShipRepairs") }, UPRShipRepairDataAsset::StaticClass(), false);
	ScanPathsForPrimaryAssets(UPRCraftingRecipeDataAsset::CraftingRecipePrimaryAssetType, { TEXT("/Game/ProjectRift/Crafting") }, UPRCraftingRecipeDataAsset::StaticClass(), false);

	ValidatePrimaryAssetType(UPRItemDataAsset::ItemPrimaryAssetType);
	ValidatePrimaryAssetType(UPRAffixDefinitionDataAsset::AffixPrimaryAssetType);
	ValidatePrimaryAssetType(UPRLootTableDataAsset::LootTablePrimaryAssetType);
	ValidatePrimaryAssetType(UPRRewardBudgetDataAsset::RewardBudgetPrimaryAssetType);
	ValidatePrimaryAssetType(UPRMissionProgressionDataAsset::MissionPrimaryAssetType);
	ValidatePrimaryAssetType(UPRRoleDataAsset::RolePrimaryAssetType);
	ValidatePrimaryAssetType(UPRRoleModuleDataAsset::RoleModulePrimaryAssetType);
	ValidatePrimaryAssetType(UPRShipRepairDataAsset::ShipRepairPrimaryAssetType);
	ValidatePrimaryAssetType(UPRCraftingRecipeDataAsset::CraftingRecipePrimaryAssetType);

	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> RoleModules;
	if (!LoadRoleCatalog(Roles, RoleModules))
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift role/module catalog failed startup validation; role abilities will remain fail-closed."));
	}
}

UPRItemDataAsset* UPRAssetManager::GetLoadedItemData(const FName ItemId) const
{
	return Cast<UPRItemDataAsset>(GetPrimaryAssetObject(MakeItemPrimaryAssetId(ItemId)));
}

UPRLootTableDataAsset* UPRAssetManager::GetLoadedLootTable(const FName AssetName) const
{
	return Cast<UPRLootTableDataAsset>(GetPrimaryAssetObject(MakeLootTablePrimaryAssetId(AssetName)));
}

UPRItemDataAsset* UPRAssetManager::LoadItemDataSync(const FName ItemId)
{
	return Cast<UPRItemDataAsset>(LoadPrimaryAssetSync(
		MakeItemPrimaryAssetId(ItemId),
		UPRItemDataAsset::StaticClass()));
}

UPRAffixDefinitionDataAsset* UPRAssetManager::LoadAffixSync(const FName AffixId)
{
	return Cast<UPRAffixDefinitionDataAsset>(LoadPrimaryAssetSync(
		MakeAffixPrimaryAssetId(AffixId),
		UPRAffixDefinitionDataAsset::StaticClass()));
}

bool UPRAssetManager::LoadAffixCatalog(TArray<UPRAffixDefinitionDataAsset*>& OutAffixes)
{
	OutAffixes.Reset();
	TArray<FPrimaryAssetId> AssetIds;
	GetPrimaryAssetIdList(UPRAffixDefinitionDataAsset::AffixPrimaryAssetType, AssetIds);
	AssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
	});
	TSet<FName> SeenIds;
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		UPRAffixDefinitionDataAsset* Affix = Cast<UPRAffixDefinitionDataAsset>(LoadPrimaryAssetSync(
			AssetId,
			UPRAffixDefinitionDataAsset::StaticClass()));
		FString Diagnostic;
		if (!Affix || SeenIds.Contains(Affix->AffixId) || !Affix->ValidateDefinition(Diagnostic))
		{
			UE_LOG(LogProjectA, Warning, TEXT("ProjectRift affix catalog entry is invalid. Asset=%s Diagnostic=%s"), *AssetId.ToString(), *Diagnostic);
			OutAffixes.Reset();
			return false;
		}
		SeenIds.Add(Affix->AffixId);
		OutAffixes.Add(Affix);
	}
	return true;
}

UPRLootTableDataAsset* UPRAssetManager::LoadLootTableSync(const FName AssetName)
{
	return Cast<UPRLootTableDataAsset>(LoadPrimaryAssetSync(
		MakeLootTablePrimaryAssetId(AssetName),
		UPRLootTableDataAsset::StaticClass()));
}

UPRRewardBudgetDataAsset* UPRAssetManager::LoadRewardBudgetSync(const FName AssetName)
{
	return Cast<UPRRewardBudgetDataAsset>(LoadPrimaryAssetSync(
		MakeRewardBudgetPrimaryAssetId(AssetName),
		UPRRewardBudgetDataAsset::StaticClass()));
}

UPRMissionProgressionDataAsset* UPRAssetManager::LoadMissionSync(const FName MissionId)
{
	return Cast<UPRMissionProgressionDataAsset>(LoadPrimaryAssetSync(
		MakeMissionPrimaryAssetId(MissionId),
		UPRMissionProgressionDataAsset::StaticClass()));
}

UPRRoleDataAsset* UPRAssetManager::LoadRoleSync(const FName RoleId)
{
	return Cast<UPRRoleDataAsset>(LoadPrimaryAssetSync(
		MakeRolePrimaryAssetId(RoleId),
		UPRRoleDataAsset::StaticClass()));
}

UPRRoleModuleDataAsset* UPRAssetManager::LoadRoleModuleSync(const FName ModuleId)
{
	return Cast<UPRRoleModuleDataAsset>(LoadPrimaryAssetSync(
		MakeRoleModulePrimaryAssetId(ModuleId),
		UPRRoleModuleDataAsset::StaticClass()));
}

bool UPRAssetManager::LoadRoleCatalog(
	TArray<UPRRoleDataAsset*>& OutRoles,
	TArray<UPRRoleModuleDataAsset*>& OutModules)
{
	OutRoles.Reset();
	OutModules.Reset();
	TArray<FPrimaryAssetId> RoleIds;
	TArray<FPrimaryAssetId> ModuleIds;
	GetPrimaryAssetIdList(UPRRoleDataAsset::RolePrimaryAssetType, RoleIds);
	GetPrimaryAssetIdList(UPRRoleModuleDataAsset::RoleModulePrimaryAssetType, ModuleIds);
	RoleIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });
	ModuleIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName); });
	for (const FPrimaryAssetId& AssetId : RoleIds)
	{
		UPRRoleDataAsset* Role = Cast<UPRRoleDataAsset>(LoadPrimaryAssetSync(AssetId, UPRRoleDataAsset::StaticClass()));
		if (!Role)
		{
			OutRoles.Reset();
			OutModules.Reset();
			return false;
		}
		OutRoles.Add(Role);
	}
	for (const FPrimaryAssetId& AssetId : ModuleIds)
	{
		UPRRoleModuleDataAsset* Module = Cast<UPRRoleModuleDataAsset>(LoadPrimaryAssetSync(AssetId, UPRRoleModuleDataAsset::StaticClass()));
		if (!Module)
		{
			OutRoles.Reset();
			OutModules.Reset();
			return false;
		}
		OutModules.Add(Module);
	}
	FString Diagnostic;
	if (!UPRRoleDataAsset::ValidateCatalog(OutRoles, OutModules, &Diagnostic))
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift role catalog is invalid: %s"), *Diagnostic);
		OutRoles.Reset();
		OutModules.Reset();
		return false;
	}
	return true;
}

UPRShipRepairDataAsset* UPRAssetManager::LoadShipRepairSync(const FName RepairProjectId)
{
	return Cast<UPRShipRepairDataAsset>(LoadPrimaryAssetSync(
		MakeShipRepairPrimaryAssetId(RepairProjectId),
		UPRShipRepairDataAsset::StaticClass()));
}

bool UPRAssetManager::LoadShipRepairCatalog(TArray<UPRShipRepairDataAsset*>& OutCatalog)
{
	OutCatalog.Reset();
	TArray<FPrimaryAssetId> AssetIds;
	GetPrimaryAssetIdList(UPRShipRepairDataAsset::ShipRepairPrimaryAssetType, AssetIds);
	AssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
	});
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		UPRShipRepairDataAsset* Contract = Cast<UPRShipRepairDataAsset>(LoadPrimaryAssetSync(
			AssetId,
			UPRShipRepairDataAsset::StaticClass()));
		if (!Contract)
		{
			OutCatalog.Reset();
			return false;
		}
		OutCatalog.Add(Contract);
	}
	FString Diagnostic;
	if (!UPRShipRepairDataAsset::ValidateCatalog(OutCatalog, &Diagnostic))
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift ship repair catalog is invalid: %s"), *Diagnostic);
		OutCatalog.Reset();
		return false;
	}
	return true;
}

UPRCraftingRecipeDataAsset* UPRAssetManager::LoadCraftingRecipeSync(const FName RecipeId)
{
	return Cast<UPRCraftingRecipeDataAsset>(LoadPrimaryAssetSync(
		MakeCraftingRecipePrimaryAssetId(RecipeId),
		UPRCraftingRecipeDataAsset::StaticClass()));
}

bool UPRAssetManager::LoadCraftingRecipeCatalog(TArray<UPRCraftingRecipeDataAsset*>& OutCatalog)
{
	OutCatalog.Reset();
	TArray<FPrimaryAssetId> AssetIds;
	GetPrimaryAssetIdList(UPRCraftingRecipeDataAsset::CraftingRecipePrimaryAssetType, AssetIds);
	AssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.PrimaryAssetName.LexicalLess(B.PrimaryAssetName);
	});
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		UPRCraftingRecipeDataAsset* Recipe = Cast<UPRCraftingRecipeDataAsset>(LoadPrimaryAssetSync(
			AssetId,
			UPRCraftingRecipeDataAsset::StaticClass()));
		if (!Recipe)
		{
			OutCatalog.Reset();
			return false;
		}
		OutCatalog.Add(Recipe);
	}
	FString Diagnostic;
	if (!UPRCraftingRecipeDataAsset::ValidateCatalog(OutCatalog, Diagnostic))
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift crafting recipe catalog is invalid: %s"), *Diagnostic);
		OutCatalog.Reset();
		return false;
	}
	return true;
}

TSharedPtr<FStreamableHandle> UPRAssetManager::LoadItemDataAsync(
	const FName ItemId,
	FPRItemDataLoadComplete Completion)
{
	const FPrimaryAssetId AssetId = MakeItemPrimaryAssetId(ItemId);
	if (!AssetId.IsValid())
	{
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	if (UPRItemDataAsset* LoadedItem = GetLoadedItemData(ItemId))
	{
		Completion.ExecuteIfBound(LoadedItem);
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAsset(
		AssetId,
		TArray<FName>(),
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, ItemId, Completion]() mutable
			{
				UPRItemDataAsset* LoadedItem = GetLoadedItemData(ItemId);
				if (!LoadedItem)
				{
					UE_LOG(LogProjectA, Warning, TEXT("Async primary item load completed without a valid object. Id=%s"), *MakeItemPrimaryAssetId(ItemId).ToString());
				}
				Completion.ExecuteIfBound(LoadedItem);
			}));

	if (!Handle)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Async primary item load did not return a handle. Id=%s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
	}

	return Handle;
}

TSharedPtr<FStreamableHandle> UPRAssetManager::LoadLootTableAsync(
	const FName AssetName,
	FPRLootTableLoadComplete Completion)
{
	const FPrimaryAssetId AssetId = MakeLootTablePrimaryAssetId(AssetName);
	if (!AssetId.IsValid())
	{
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	if (UPRLootTableDataAsset* LoadedLootTable = GetLoadedLootTable(AssetName))
	{
		Completion.ExecuteIfBound(LoadedLootTable);
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
		return nullptr;
	}

	TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAsset(
		AssetId,
		TArray<FName>(),
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, AssetName, Completion]() mutable
			{
				UPRLootTableDataAsset* LoadedLootTable = GetLoadedLootTable(AssetName);
				if (!LoadedLootTable)
				{
					UE_LOG(LogProjectA, Warning, TEXT("Async primary loot table load completed without a valid object. Id=%s"), *MakeLootTablePrimaryAssetId(AssetName).ToString());
				}
				Completion.ExecuteIfBound(LoadedLootTable);
			}));

	if (!Handle)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Async primary loot table load did not return a handle. Id=%s"), *AssetId.ToString());
		Completion.ExecuteIfBound(nullptr);
	}

	return Handle;
}

UObject* UPRAssetManager::LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass)
{
	if (!AssetId.IsValid() || !ExpectedClass)
	{
		return nullptr;
	}

	if (UObject* Existing = GetPrimaryAssetObject(AssetId))
	{
		if (Existing->IsA(ExpectedClass))
		{
			return Existing;
		}

		UE_LOG(
			LogProjectA,
			Warning,
			TEXT("Loaded primary asset has the wrong type. Id=%s Expected=%s Actual=%s"),
			*AssetId.ToString(),
			*GetNameSafe(ExpectedClass),
			*GetNameSafe(Existing->GetClass()));
		return nullptr;
	}

	const FSoftObjectPath AssetPath = GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Primary asset is not registered: %s"), *AssetId.ToString());
		return nullptr;
	}

	UObject* Loaded = AssetPath.TryLoad();
	if (!Loaded || !Loaded->IsA(ExpectedClass))
	{
		UE_LOG(
			LogProjectA,
			Warning,
			TEXT("Primary asset load/type validation failed. Id=%s Path=%s Expected=%s Actual=%s"),
			*AssetId.ToString(),
			*AssetPath.ToString(),
			*GetNameSafe(ExpectedClass),
			*GetNameSafe(Loaded ? Loaded->GetClass() : nullptr));
		return nullptr;
	}

	return Loaded;
}

void UPRAssetManager::ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const
{
	TArray<FPrimaryAssetId> AssetIds;
	GetPrimaryAssetIdList(AssetType, AssetIds);
	if (AssetIds.IsEmpty())
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift primary asset type has no registered assets. Type=%s"), *AssetType.ToString());
		return;
	}

	UE_LOG(LogProjectA, Log, TEXT("ProjectRift primary assets registered. Type=%s Count=%d"), *AssetType.ToString(), AssetIds.Num());
}
