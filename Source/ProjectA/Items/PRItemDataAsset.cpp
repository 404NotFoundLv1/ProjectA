#include "Items/PRItemDataAsset.h"

const FPrimaryAssetType UPRItemDataAsset::ItemPrimaryAssetType(TEXT("ProjectRiftItem"));

FPrimaryAssetId UPRItemDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ItemPrimaryAssetType, ItemId.IsNone() ? GetFName() : ItemId);
}

bool UPRItemDataAsset::IsStackable() const
{
	return MaxStackCount > 1;
}
