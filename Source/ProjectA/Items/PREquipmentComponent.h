#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Items/PREquipmentTypes.h"
#include "Persistence/PRProfileTypes.h"
#include "PREquipmentComponent.generated.h"

class UPRAbilitySystemComponent;
class UPREquipmentDataAsset;
class APREquipmentVisualActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPREquipmentChangedDelegate);

USTRUCT()
struct PROJECTA_API FPREquipmentRuntimeHandles
{
	GENERATED_BODY()

	TArray<FActiveGameplayEffectHandle> ActiveEffectHandles;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};

/**
 * PlayerState-owned authority for persistent equipment and its GAS grants.
 */
UCLASS(BlueprintType, ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPREquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPREquipmentComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FPREquipmentChangedDelegate OnEquipmentChanged;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FPRItemInstance GetEquippedItem(FName SlotId) const;

	const TArray<FPRProfileEquipmentEntry>& GetEquipmentEntries() const { return EquipmentEntries; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<FPREquipmentAppearanceEntry> GetPublicAppearanceEntries() const { return PublicAppearanceEntries; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ReplaceEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment|GAS")
	bool RefreshGrantedHandles();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment|GAS")
	void ClearGrantedHandles();

	void CopyRuntimeStateFrom(const UPREquipmentComponent* SourceComponent);
	void HandleAvatarChanged();

	static FName GetSlotId(EPREquipmentSlot Slot);

private:
	UFUNCTION()
	void OnRep_EquipmentEntries();

	UFUNCTION()
	void OnRep_PublicAppearanceEntries();

	bool ValidateEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic) const;
	bool HasMatchingGrantedHandles() const;
	bool ApplyGrantsForEntry(const FPRProfileEquipmentEntry& Entry, const UPREquipmentDataAsset* Definition, FPREquipmentRuntimeHandles& OutHandles);
	void RemoveHandles(const FPREquipmentRuntimeHandles& Handles);
	void RebuildPublicAppearanceEntries();
	void RefreshVisualActors();
	void DestroyVisualActors();
	void NotifyEquipmentChanged();
	UPRAbilitySystemComponent* GetAbilitySystemComponent() const;
	const UPREquipmentDataAsset* ResolveEquipmentDefinition(const FPRItemInstance& Item) const;

	UPROPERTY(ReplicatedUsing = OnRep_EquipmentEntries, VisibleInstanceOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FPRProfileEquipmentEntry> EquipmentEntries;

	UPROPERTY(ReplicatedUsing = OnRep_PublicAppearanceEntries, VisibleInstanceOnly, BlueprintReadOnly, Category = "Equipment|Appearance", meta = (AllowPrivateAccess = "true"))
	TArray<FPREquipmentAppearanceEntry> PublicAppearanceEntries;

	UPROPERTY(Transient)
	TMap<FGuid, FPREquipmentRuntimeHandles> RuntimeHandlesByInstance;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<APREquipmentVisualActor>> SpawnedVisualActors;
};
