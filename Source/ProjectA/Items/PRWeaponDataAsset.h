#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/PRItemDataAsset.h"
#include "PRWeaponDataAsset.generated.h"

class APRWeaponActor;

/** Data-driven firearm definition stored in the existing ProjectRiftItem registry. */
UCLASS(BlueprintType)
class PROJECTA_API UPRWeaponDataAsset : public UPRItemDataAsset
{
	GENERATED_BODY()

public:
	UPRWeaponDataAsset();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	FName AmmoItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo", meta = (ClampMin = "0"))
	int32 InitialReserveAmmo = 48;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	FGameplayTag DamageType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Firing", meta = (ClampMin = "0.0"))
	float Range = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Firing", meta = (ClampMin = "0.0"))
	float MinFireInterval = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Firing", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float HipSpreadDegrees = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Firing", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float AimSpreadDegrees = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload", meta = (ClampMin = "0.0"))
	float ReloadDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim", meta = (ClampMin = "0.0"))
	float AimArmLength = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float AimFieldOfView = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FName AttachSocketName = TEXT("HandGrip_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FTransform AttachTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	TSubclassOf<APRWeaponActor> WeaponActorClass;
};
