#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "PRAssetManager.generated.h"

class UPRItemDataAsset;
class UPRLootTableDataAsset;
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
	static FPrimaryAssetId MakeLootTablePrimaryAssetId(FName AssetName);

	virtual void StartInitialLoading() override;

	UPRItemDataAsset* GetLoadedItemData(FName ItemId) const;
	UPRLootTableDataAsset* GetLoadedLootTable(FName AssetName) const;
	UPRItemDataAsset* LoadItemDataSync(FName ItemId);
	UPRLootTableDataAsset* LoadLootTableSync(FName AssetName);
	TSharedPtr<FStreamableHandle> LoadItemDataAsync(FName ItemId, FPRItemDataLoadComplete Completion);
	TSharedPtr<FStreamableHandle> LoadLootTableAsync(FName AssetName, FPRLootTableLoadComplete Completion);

private:
	UObject* LoadPrimaryAssetSync(const FPrimaryAssetId& AssetId, UClass* ExpectedClass);
	void ValidatePrimaryAssetType(const FPrimaryAssetType& AssetType) const;
};
