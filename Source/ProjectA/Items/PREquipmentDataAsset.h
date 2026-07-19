#pragma once

#include "CoreMinimal.h"
#include "Items/PREquipmentTypes.h"
#include "Items/PRItemDataAsset.h"
#include "PREquipmentDataAsset.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class APREquipmentVisualActor;

/** Definition shared by all fixed-slot equipment items. */
UCLASS(BlueprintType)
class PROJECTA_API UPREquipmentDataAsset : public UPRItemDataAsset
{
	GENERATED_BODY()

public:
	UPREquipmentDataAsset();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EPREquipmentSlot EquipmentSlot = EPREquipmentSlot::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|GAS")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|GAS")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Appearance")
	TSubclassOf<APREquipmentVisualActor> VisualActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Appearance")
	FName AttachmentSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Appearance")
	FTransform AttachmentTransform = FTransform::Identity;
};
