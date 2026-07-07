#include "Items/PRItemDataLibrary.h"

#include "Items/PRItemDataAsset.h"

UPRItemDataAsset* UPRItemDataLibrary::FindItemDataById(const TArray<UPRItemDataAsset*>& ItemDataAssets, const FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}

	for (UPRItemDataAsset* ItemData : ItemDataAssets)
	{
		if (ItemData && ItemData->ItemId == ItemId)
		{
			return ItemData;
		}
	}

	return nullptr;
}

bool UPRItemDataLibrary::IsValidItemInstance(const FPRItemInstance& ItemInstance)
{
	return ItemInstance.IsValid();
}
