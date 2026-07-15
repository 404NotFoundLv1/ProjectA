#include "Items/PRWeaponDataAsset.h"

#include "Core/PRGameplayTags.h"
#include "Weapons/PRWeaponActor.h"

UPRWeaponDataAsset::UPRWeaponDataAsset()
{
	ItemType = EPRItemType::Equipment;
	MaxStackCount = 1;
	bCanUse = false;
	bCanEquip = true;
	bCanDrop = false;
	DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	WeaponActorClass = APRWeaponActor::StaticClass();
}
