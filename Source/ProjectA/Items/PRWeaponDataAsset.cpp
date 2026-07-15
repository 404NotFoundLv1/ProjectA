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
	HitReaction.Strength = EPRHitReactionStrength::Light;
	HitReaction.DurationSeconds = 0.12f;
	WeaponActorClass = APRWeaponActor::StaticClass();
}
