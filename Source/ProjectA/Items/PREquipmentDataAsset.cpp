#include "Items/PREquipmentDataAsset.h"

UPREquipmentDataAsset::UPREquipmentDataAsset()
{
	ItemType = EPRItemType::Equipment;
	MaxStackCount = 1;
	bCanEquip = true;
}
