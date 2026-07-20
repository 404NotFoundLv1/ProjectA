#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ActorComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GA_PrimaryAttack.h"
#include "Abilities/PREquipmentGameplayEffects.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRTemporaryShieldGameplayEffect.h"
#include "Core/PRAssetManager.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PREquipmentComponent.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemTransactionComponent.h"
#include "Items/PRItemTransactionTypes.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREquipmentRuntimeTest,
	"ProjectRift.Equipment.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
void RequireEquipmentRuntimeFunction(FAutomationTestBase& Test, UClass* Owner, const TCHAR* Name)
{
	Test.TestNotNull(FString::Printf(TEXT("Equipment component exposes %s"), Name), Owner ? Owner->FindFunctionByName(Name) : nullptr);
}

int32 CountActiveEffectsByClass(const UAbilitySystemComponent& AbilitySystemComponent, UClass* EffectClass)
{
	FGameplayEffectQuery Query;
	Query.EffectDefinition = EffectClass;
	return AbilitySystemComponent.GetActiveEffects(Query).Num();
}

int32 CountGrantedAbilitiesBySource(const UAbilitySystemComponent& AbilitySystemComponent, const UObject* SourceObject)
{
	int32 Count = 0;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (Spec.SourceObject == SourceObject)
		{
			++Count;
		}
	}
	return Count;
}
}

bool FPREquipmentRuntimeTest::RunTest(const FString& Parameters)
{
	UClass* EquipmentComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREquipmentComponent"));
	TestNotNull(TEXT("Equipment component is reflected"), EquipmentComponentClass);
	if (!EquipmentComponentClass)
	{
		return false;
	}

	const UActorComponent* ComponentCDO = Cast<UActorComponent>(EquipmentComponentClass->GetDefaultObject());
	TestTrue(TEXT("Equipment component replicates"), ComponentCDO && ComponentCDO->GetIsReplicated());
	RequireEquipmentRuntimeFunction(*this, EquipmentComponentClass, TEXT("GetEquippedItem"));
	RequireEquipmentRuntimeFunction(*this, EquipmentComponentClass, TEXT("GetPublicAppearanceEntries"));
	RequireEquipmentRuntimeFunction(*this, EquipmentComponentClass, TEXT("ReplaceEquipmentEntries"));
	RequireEquipmentRuntimeFunction(*this, EquipmentComponentClass, TEXT("RefreshGrantedHandles"));
	RequireEquipmentRuntimeFunction(*this, EquipmentComponentClass, TEXT("ClearGrantedHandles"));

	const FArrayProperty* PrivateEntries = FindFProperty<FArrayProperty>(EquipmentComponentClass, TEXT("EquipmentEntries"));
	const FArrayProperty* PublicEntries = FindFProperty<FArrayProperty>(EquipmentComponentClass, TEXT("PublicAppearanceEntries"));
	TestNotNull(TEXT("Equipment component stores private equipment entries"), PrivateEntries);
	TestNotNull(TEXT("Equipment component stores public appearance entries"), PublicEntries);

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Equipment runtime test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Equipment runtime test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	TestNotNull(TEXT("Runtime PlayerState is created"), PlayerState);
	if (!PlayerState)
	{
		return false;
	}
	UPREquipmentComponent* EquipmentComponent = PlayerState->GetEquipmentComponent();
	UPRInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent();
	UPRItemTransactionComponent* TransactionComponent = PlayerState->GetItemTransactionComponent();
	UPRAttributeSet* AttributeSet = PlayerState->GetAttributeSet();
	UAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetAbilitySystemComponent();
	TestNotNull(TEXT("Runtime equipment component is available"), EquipmentComponent);
	TestNotNull(TEXT("Runtime inventory component is available"), InventoryComponent);
	TestNotNull(TEXT("Runtime transaction component is available"), TransactionComponent);
	TestNotNull(TEXT("Runtime attributes are available"), AttributeSet);
	TestNotNull(TEXT("Runtime ability system is available"), AbilitySystemComponent);
	if (!EquipmentComponent || !InventoryComponent || !TransactionComponent || !AbilitySystemComponent || !AttributeSet)
	{
		return false;
	}
	AbilitySystemComponent->InitAbilityActorInfo(PlayerState, PlayerState);

	UPREquipmentDataAsset* ArmorDefinition = NewObject<UPREquipmentDataAsset>(PlayerState);
	ArmorDefinition->ItemId = TEXT("RuntimeArmor");
	ArmorDefinition->ItemType = EPRItemType::Equipment;
	ArmorDefinition->bCanEquip = true;
	ArmorDefinition->EquipmentSlot = EPREquipmentSlot::Armor;
	ArmorDefinition->GrantedEffects.Add(UPRTestArmorEquipmentGameplayEffect::StaticClass());
	ArmorDefinition->GrantedEffects.Add(UPRTemporaryShieldGameplayEffect::StaticClass());
	ArmorDefinition->GrantedAbilities.Add(UGA_PrimaryAttack::StaticClass());
	InventoryComponent->SetItemDataAssets({ ArmorDefinition });

	FPRProfileEquipmentEntry ArmorEntry;
	ArmorEntry.SlotId = ProjectRiftEquipmentSlots::Armor;
	ArmorEntry.Item.InstanceGuid = FGuid::NewGuid();
	ArmorEntry.Item.ItemId = ArmorDefinition->ItemId;
	ArmorEntry.Item.Count = 1;
	AttributeSet->SetMaxHealth(100.0f);
	AttributeSet->SetHealth(50.0f);
	FString Diagnostic;
	TestTrue(TEXT("A matching equipment definition can be equipped"), EquipmentComponent->ReplaceEquipmentEntries({ ArmorEntry }, Diagnostic));
	TestEqual(TEXT("Armor increases maximum health"), AttributeSet->GetMaxHealth(), 125.0f);
	TestEqual(TEXT("Armor preserves the current health percentage"), AttributeSet->GetHealth(), 62.5f);
	TestEqual(TEXT("Equipment applies exactly one granted effect"), CountActiveEffectsByClass(*AbilitySystemComponent, UPRTemporaryShieldGameplayEffect::StaticClass()), 1);
	TestEqual(TEXT("Equipment grants exactly one ability"), CountGrantedAbilitiesBySource(*AbilitySystemComponent, ArmorDefinition), 1);
	TestEqual(TEXT("Equipment publishes only the equipped visual summary"), EquipmentComponent->GetPublicAppearanceEntries().Num(), 1);
	TestTrue(TEXT("Refreshing unchanged equipment does not stack grants"), EquipmentComponent->RefreshGrantedHandles());
	TestEqual(TEXT("Refresh keeps one granted effect"), CountActiveEffectsByClass(*AbilitySystemComponent, UPRTemporaryShieldGameplayEffect::StaticClass()), 1);
	TestEqual(TEXT("Refresh keeps one granted ability"), CountGrantedAbilitiesBySource(*AbilitySystemComponent, ArmorDefinition), 1);

	TestTrue(TEXT("Unequipping removes tracked grants"), EquipmentComponent->ReplaceEquipmentEntries({}, Diagnostic));
	TestEqual(TEXT("Unequipping restores maximum health"), AttributeSet->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Unequipping preserves the current health percentage"), AttributeSet->GetHealth(), 50.0f);
	TestEqual(TEXT("Unequip removes granted effects"), CountActiveEffectsByClass(*AbilitySystemComponent, UPRTemporaryShieldGameplayEffect::StaticClass()), 0);
	TestEqual(TEXT("Unequip removes granted abilities"), CountGrantedAbilitiesBySource(*AbilitySystemComponent, ArmorDefinition), 0);

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TArray<UPRAffixDefinitionDataAsset*> AffixCatalog;
	TestTrue(TEXT("Runtime affix test loads the validated catalog"), AssetManager && AssetManager->LoadAffixCatalog(AffixCatalog));
	UPRAffixDefinitionDataAsset* const* VitalAffixEntry = AffixCatalog.FindByPredicate([](const UPRAffixDefinitionDataAsset* Candidate)
	{
		return Candidate
			&& Candidate->AllowedSlots.Contains(EPREquipmentSlot::Armor)
			&& Candidate->Attribute == EPRAffixAttribute::MaxHealth
			&& Candidate->ModifierType == EPRAffixModifierType::Additive;
	});
	UPRAffixDefinitionDataAsset* VitalAffix = VitalAffixEntry ? *VitalAffixEntry : nullptr;
	TestNotNull(TEXT("Catalog provides an additive MaxHealth armor affix"), VitalAffix);
	if (!VitalAffix)
	{
		return false;
	}

	UPREquipmentDataAsset* AffixArmorDefinition = NewObject<UPREquipmentDataAsset>(PlayerState);
	AffixArmorDefinition->ItemId = TEXT("RuntimeAffixArmor");
	AffixArmorDefinition->ItemType = EPRItemType::Equipment;
	AffixArmorDefinition->bCanEquip = true;
	AffixArmorDefinition->EquipmentSlot = EPREquipmentSlot::Armor;
	InventoryComponent->SetItemDataAssets({ ArmorDefinition, AffixArmorDefinition });
	FPRProfileEquipmentEntry AffixArmorEntry;
	AffixArmorEntry.SlotId = ProjectRiftEquipmentSlots::Armor;
	AffixArmorEntry.Item.InstanceGuid = FGuid::NewGuid();
	AffixArmorEntry.Item.ItemId = AffixArmorDefinition->ItemId;
	AffixArmorEntry.Item.Count = 1;
	AffixArmorEntry.Item.LootSeed = 1701;
	AffixArmorEntry.Item.AffixGenerationVersion = 1;
	AffixArmorEntry.Item.Affixes = { VitalAffix->AffixId };
	AffixArmorEntry.Item.RolledAffixes = {
		FPRAffixRoll{ VitalAffix->AffixId, VitalAffix->Attribute, VitalAffix->ModifierType, VitalAffix->MinMagnitude }
	};
	AttributeSet->SetMaxHealth(100.0f);
	AttributeSet->SetHealth(50.0f);
	TestTrue(TEXT("A final affix roll can be equipped"), EquipmentComponent->ReplaceEquipmentEntries({ AffixArmorEntry }, Diagnostic));
	TestEqual(TEXT("Additive MaxHealth affix changes the aggregated maximum"), AttributeSet->GetMaxHealth(), 100.0f + VitalAffix->MinMagnitude);
	TestEqual(TEXT("Additive MaxHealth affix preserves health percentage"), AttributeSet->GetHealth(), 50.0f + VitalAffix->MinMagnitude * 0.5f);
	TestEqual(TEXT("Exactly one native affix effect is active"), CountActiveEffectsByClass(*AbilitySystemComponent, VitalAffix->ModifierEffectClass), 1);
	TestTrue(TEXT("Unequipping final affix roll removes its tracked handle"), EquipmentComponent->ReplaceEquipmentEntries({}, Diagnostic));
	TestEqual(TEXT("Unequipping final affix restores maximum health"), AttributeSet->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Unequipping final affix restores current health ratio"), AttributeSet->GetHealth(), 50.0f);

	UPRAffixDefinitionDataAsset* const* VitalPercentAffixEntry = AffixCatalog.FindByPredicate([](const UPRAffixDefinitionDataAsset* Candidate)
	{
		return Candidate
			&& Candidate->AllowedSlots.Contains(EPREquipmentSlot::Armor)
			&& Candidate->Attribute == EPRAffixAttribute::MaxHealth
			&& Candidate->ModifierType == EPRAffixModifierType::Percentage;
	});
	UPRAffixDefinitionDataAsset* VitalPercentAffix = VitalPercentAffixEntry ? *VitalPercentAffixEntry : nullptr;
	TestNotNull(TEXT("Catalog provides a percentage MaxHealth armor affix"), VitalPercentAffix);
	if (!VitalPercentAffix)
	{
		return false;
	}
	FPRProfileEquipmentEntry PercentAffixArmorEntry = AffixArmorEntry;
	PercentAffixArmorEntry.Item.InstanceGuid = FGuid::NewGuid();
	PercentAffixArmorEntry.Item.Affixes = { VitalPercentAffix->AffixId };
	PercentAffixArmorEntry.Item.RolledAffixes = {
		FPRAffixRoll{ VitalPercentAffix->AffixId, VitalPercentAffix->Attribute, VitalPercentAffix->ModifierType, VitalPercentAffix->MinMagnitude }
	};
	TestTrue(TEXT("A percentage final affix roll can be equipped"), EquipmentComponent->ReplaceEquipmentEntries({ PercentAffixArmorEntry }, Diagnostic));
	TestTrue(TEXT("Percentage MaxHealth affix applies its explicit multiplier"), FMath::IsNearlyEqual(AttributeSet->GetMaxHealth(), 100.0f * (1.0f + VitalPercentAffix->MinMagnitude)));
	TestTrue(TEXT("Percentage MaxHealth affix preserves health percentage"), FMath::IsNearlyEqual(AttributeSet->GetHealth(), 50.0f * (1.0f + VitalPercentAffix->MinMagnitude)));
	TestTrue(TEXT("Unequipping percentage final affix removes its tracked handle"), EquipmentComponent->ReplaceEquipmentEntries({}, Diagnostic));
	TestEqual(TEXT("Unequipping percentage final affix restores maximum health"), AttributeSet->GetMaxHealth(), 100.0f);

	FPRProfileEquipmentEntry UnknownAffixEntry = AffixArmorEntry;
	UnknownAffixEntry.Item.InstanceGuid = FGuid::NewGuid();
	UnknownAffixEntry.Item.Affixes = { TEXT("Affix.Future.Unknown") };
	UnknownAffixEntry.Item.RolledAffixes = {
		FPRAffixRoll{ TEXT("Affix.Future.Unknown"), EPRAffixAttribute::MaxHealth, EPRAffixModifierType::Additive, 99.0f }
	};
	TestTrue(TEXT("Unknown final affix remains equipable without inventing a grant"), EquipmentComponent->ReplaceEquipmentEntries({ UnknownAffixEntry }, Diagnostic));
	TestEqual(TEXT("Unknown final affix grants no MaxHealth modification"), AttributeSet->GetMaxHealth(), 100.0f);
	TestTrue(TEXT("Unknown final affix can be unequipped safely"), EquipmentComponent->ReplaceEquipmentEntries({}, Diagnostic));

	ArmorEntry.SlotId = ProjectRiftEquipmentSlots::Chip;
	TestFalse(TEXT("A mismatched slot is rejected"), EquipmentComponent->ReplaceEquipmentEntries({ ArmorEntry }, Diagnostic));
	TestTrue(TEXT("Rejected replacement preserves the empty equipment state"), EquipmentComponent->GetEquipmentEntries().IsEmpty());

	ArmorEntry.SlotId = ProjectRiftEquipmentSlots::Armor;
	TestTrue(TEXT("Inventory accepts an equipment instance for transaction testing"), InventoryComponent->ReplaceInventoryItems({ ArmorEntry.Item }));
	FPRItemTransactionRequest EquipRequest;
	EquipRequest.Header.TransactionId = FGuid::NewGuid();
	EquipRequest.Header.ExpectedRevision = TransactionComponent->GetRevision();
	EquipRequest.Intent = EPRItemTransactionIntent::Equip;
	EquipRequest.InstanceGuid = ArmorEntry.Item.InstanceGuid;
	EquipRequest.SlotId = ProjectRiftEquipmentSlots::Armor;
	const FPRItemTransactionResult EquipResult = TransactionComponent->ExecuteAuthoritativeTransaction(EquipRequest);
	TestEqual(TEXT("Generic armor equip is a successful authoritative transaction"), EquipResult.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Generic armor equip removes the inventory item"), InventoryComponent->GetInventoryItems().Num(), 0);
	TestEqual(TEXT("Generic armor equip populates the requested slot"), EquipmentComponent->GetEquipmentEntries().Num(), 1);

	FPRItemTransactionRequest UnequipRequest;
	UnequipRequest.Header.TransactionId = FGuid::NewGuid();
	UnequipRequest.Header.ExpectedRevision = TransactionComponent->GetRevision();
	UnequipRequest.Intent = EPRItemTransactionIntent::Unequip;
	UnequipRequest.SlotId = ProjectRiftEquipmentSlots::Armor;
	const FPRItemTransactionResult UnequipResult = TransactionComponent->ExecuteAuthoritativeTransaction(UnequipRequest);
	TestEqual(TEXT("Generic armor unequip is a successful authoritative transaction"), UnequipResult.Status, EPRItemTransactionStatus::Success);
	TestEqual(TEXT("Generic armor unequip restores the inventory item"), InventoryComponent->GetInventoryItems().Num(), 1);
	TestTrue(TEXT("Generic armor unequip clears the requested slot"), EquipmentComponent->GetEquipmentEntries().IsEmpty());

	TestTrue(TEXT("Replacement setup removes the source inventory copy"), InventoryComponent->ReplaceInventoryItems({}));
	TestTrue(TEXT("Source can restore the equipped armor before PlayerState replacement"), EquipmentComponent->ReplaceEquipmentEntries({ ArmorEntry }, Diagnostic));
	APRPlayerState* ReplacementPlayerState = World->SpawnActor<APRPlayerState>();
	TestNotNull(TEXT("Replacement PlayerState is created"), ReplacementPlayerState);
	if (!ReplacementPlayerState)
	{
		return false;
	}
	UPREquipmentComponent* ReplacementEquipment = ReplacementPlayerState->GetEquipmentComponent();
	UPRInventoryComponent* ReplacementInventory = ReplacementPlayerState->GetInventoryComponent();
	UAbilitySystemComponent* ReplacementAbilitySystem = ReplacementPlayerState->GetAbilitySystemComponent();
	TestNotNull(TEXT("Replacement equipment component is available"), ReplacementEquipment);
	TestNotNull(TEXT("Replacement inventory is available"), ReplacementInventory);
	TestNotNull(TEXT("Replacement ability system is available"), ReplacementAbilitySystem);
	if (!ReplacementEquipment || !ReplacementInventory || !ReplacementAbilitySystem)
	{
		return false;
	}
	ReplacementInventory->SetItemDataAssets({ ArmorDefinition });
	ReplacementAbilitySystem->InitAbilityActorInfo(ReplacementPlayerState, ReplacementPlayerState);
	ReplacementEquipment->CopyRuntimeStateFrom(EquipmentComponent);
	TestEqual(TEXT("PlayerState copy transfers equipment without copying stale GAS handles"),
		CountActiveEffectsByClass(*ReplacementAbilitySystem, UPRTemporaryShieldGameplayEffect::StaticClass()), 0);
	ReplacementEquipment->HandleAvatarChanged();
	TestEqual(TEXT("Replacement avatar applies one equipment effect"),
		CountActiveEffectsByClass(*ReplacementAbilitySystem, UPRTemporaryShieldGameplayEffect::StaticClass()), 1);
	TestEqual(TEXT("Replacement avatar grants one equipment ability"),
		CountGrantedAbilitiesBySource(*ReplacementAbilitySystem, ArmorDefinition), 1);
	ReplacementEquipment->HandleAvatarChanged();
	TestEqual(TEXT("Repeated replacement avatar initialization does not duplicate effects"),
		CountActiveEffectsByClass(*ReplacementAbilitySystem, UPRTemporaryShieldGameplayEffect::StaticClass()), 1);
	TestEqual(TEXT("Repeated replacement avatar initialization does not duplicate abilities"),
		CountGrantedAbilitiesBySource(*ReplacementAbilitySystem, ArmorDefinition), 1);
	return true;
}

#endif
