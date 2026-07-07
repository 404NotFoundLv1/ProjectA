#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ActorComponent.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRInventoryComponentTest, "ProjectRift.Items.InventoryComponent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UScriptStruct* GetItemInstanceStruct()
{
	return FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRItemInstance"));
}

void SetItemInstance(void* ItemMemory, const FName ItemId, const int32 Count)
{
	UScriptStruct* ItemInstanceStruct = GetItemInstanceStruct();
	if (!ItemMemory || !ItemInstanceStruct)
	{
		return;
	}

	if (FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(ItemInstanceStruct, TEXT("ItemId")))
	{
		ItemIdProperty->SetPropertyValue_InContainer(ItemMemory, ItemId);
	}

	if (FIntProperty* CountProperty = FindFProperty<FIntProperty>(ItemInstanceStruct, TEXT("Count")))
	{
		CountProperty->SetPropertyValue_InContainer(ItemMemory, Count);
	}
}

void SetIntProperty(UObject* Object, const TCHAR* PropertyName, const int32 Value)
{
	if (FIntProperty* Property = Object ? FindFProperty<FIntProperty>(Object->GetClass(), PropertyName) : nullptr)
	{
		Property->SetPropertyValue_InContainer(Object, Value);
	}
}

FPRItemInstance MakeInventoryComponentTestItem(
	const FName ItemId,
	const int32 Count,
	const int32 Level = 1,
	const EPRItemRarity Rarity = EPRItemRarity::Common,
	const float Durability = 1.0f,
	const TArray<FName>& Affixes = TArray<FName>())
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Level = Level;
	Item.Rarity = Rarity;
	Item.Durability = Durability;
	Item.Affixes = Affixes;
	return Item;
}

bool CallSetItemDataAssets(FAutomationTestBase& Test, UObject* Inventory, const TArray<UPRItemDataAsset*>& ItemDataAssets)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("SetItemDataAssets")) : nullptr;
	Test.TestNotNull(TEXT("SetItemDataAssets function exists"), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	FArrayProperty* AssetsProperty = FindFProperty<FArrayProperty>(Function, TEXT("InItemDataAssets"));
	FObjectProperty* InnerObjectProperty = AssetsProperty ? CastField<FObjectProperty>(AssetsProperty->Inner) : nullptr;
	Test.TestNotNull(TEXT("SetItemDataAssets accepts an item data asset array"), AssetsProperty);
	Test.TestNotNull(TEXT("SetItemDataAssets array stores item data objects"), InnerObjectProperty);
	if (!AssetsProperty || !InnerObjectProperty)
	{
		return false;
	}

	FScriptArrayHelper AssetsHelper(AssetsProperty, AssetsProperty->ContainerPtrToValuePtr<void>(ParamsMemory));
	for (UPRItemDataAsset* ItemDataAsset : ItemDataAssets)
	{
		const int32 NewIndex = AssetsHelper.AddValue();
		InnerObjectProperty->SetObjectPropertyValue(AssetsHelper.GetRawPtr(NewIndex), ItemDataAsset);
	}

	Inventory->ProcessEvent(Function, ParamsMemory);
	return true;
}

int32 CallGetMaxStackCount(FAutomationTestBase& Test, UObject* Inventory, const FName ItemId)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("GetMaxStackCount")) : nullptr;
	Test.TestNotNull(TEXT("GetMaxStackCount function exists"), Function);
	if (!Inventory || !Function)
	{
		return INDEX_NONE;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(Function, TEXT("ItemId"));
	Test.TestNotNull(TEXT("GetMaxStackCount accepts an ItemId"), ItemIdProperty);
	if (!ItemIdProperty)
	{
		return INDEX_NONE;
	}

	ItemIdProperty->SetPropertyValue_InContainer(ParamsMemory, ItemId);
	Inventory->ProcessEvent(Function, ParamsMemory);

	FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(Function, TEXT("ReturnValue"));
	Test.TestNotNull(TEXT("GetMaxStackCount returns int32"), ReturnProperty);
	return ReturnProperty ? ReturnProperty->GetPropertyValue_InContainer(ParamsMemory) : INDEX_NONE;
}

bool CallItemFunction(FAutomationTestBase& Test, UObject* Inventory, const TCHAR* FunctionName, const FName ItemId, const int32 Count)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(FName(FunctionName)) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s function exists"), FunctionName), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	FStructProperty* ItemProperty = FindFProperty<FStructProperty>(Function, TEXT("Item"));
	Test.TestNotNull(FString::Printf(TEXT("%s accepts FPRItemInstance Item"), FunctionName), ItemProperty);
	if (!ItemProperty)
	{
		return false;
	}

	void* ItemMemory = ItemProperty->ContainerPtrToValuePtr<void>(ParamsMemory);
	SetItemInstance(ItemMemory, ItemId, Count);

	Inventory->ProcessEvent(Function, ParamsMemory);

	FBoolProperty* ReturnProperty = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue"));
	Test.TestNotNull(FString::Printf(TEXT("%s returns bool"), FunctionName), ReturnProperty);
	return ReturnProperty && ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
}

bool CallNamedCountFunction(FAutomationTestBase& Test, UObject* Inventory, const TCHAR* FunctionName, const FName ItemId, const int32 Count)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(FName(FunctionName)) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s function exists"), FunctionName), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	if (FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(Function, TEXT("ItemId")))
	{
		ItemIdProperty->SetPropertyValue_InContainer(ParamsMemory, ItemId);
	}
	else
	{
		Test.AddError(FString::Printf(TEXT("%s is missing ItemId parameter"), FunctionName));
		return false;
	}

	if (FIntProperty* CountProperty = FindFProperty<FIntProperty>(Function, TEXT("Count")))
	{
		CountProperty->SetPropertyValue_InContainer(ParamsMemory, Count);
	}
	else
	{
		Test.AddError(FString::Printf(TEXT("%s is missing Count parameter"), FunctionName));
		return false;
	}

	Inventory->ProcessEvent(Function, ParamsMemory);

	FBoolProperty* ReturnProperty = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue"));
	Test.TestNotNull(FString::Printf(TEXT("%s returns bool"), FunctionName), ReturnProperty);
	return ReturnProperty && ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
}

bool CallUseItem(FAutomationTestBase& Test, UObject* Inventory, const FName ItemId)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("UseItem")) : nullptr;
	Test.TestNotNull(TEXT("UseItem function exists"), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	if (FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(Function, TEXT("ItemId")))
	{
		ItemIdProperty->SetPropertyValue_InContainer(ParamsMemory, ItemId);
	}
	else
	{
		Test.AddError(TEXT("UseItem is missing ItemId parameter"));
		return false;
	}

	Inventory->ProcessEvent(Function, ParamsMemory);

	FBoolProperty* ReturnProperty = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue"));
	Test.TestNotNull(TEXT("UseItem returns bool"), ReturnProperty);
	return ReturnProperty && ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
}

int32 CallGetItemCount(FAutomationTestBase& Test, UObject* Inventory, const FName ItemId)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("GetItemCount")) : nullptr;
	Test.TestNotNull(TEXT("GetItemCount function exists"), Function);
	if (!Inventory || !Function)
	{
		return INDEX_NONE;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	if (FNameProperty* ItemIdProperty = FindFProperty<FNameProperty>(Function, TEXT("ItemId")))
	{
		ItemIdProperty->SetPropertyValue_InContainer(ParamsMemory, ItemId);
	}
	else
	{
		Test.AddError(TEXT("GetItemCount is missing ItemId parameter"));
		return INDEX_NONE;
	}

	Inventory->ProcessEvent(Function, ParamsMemory);

	FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(Function, TEXT("ReturnValue"));
	Test.TestNotNull(TEXT("GetItemCount returns int32"), ReturnProperty);
	return ReturnProperty ? ReturnProperty->GetPropertyValue_InContainer(ParamsMemory) : INDEX_NONE;
}

}

bool FPRInventoryComponentTest::RunTest(const FString& Parameters)
{
	UClass* InventoryComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRInventoryComponent"));
	TestNotNull(TEXT("UPRInventoryComponent class exists"), InventoryComponentClass);
	TestTrue(
		TEXT("UPRInventoryComponent derives from UActorComponent"),
		InventoryComponentClass && InventoryComponentClass->IsChildOf(UActorComponent::StaticClass()));

	UScriptStruct* ItemInstanceStruct = GetItemInstanceStruct();
	TestNotNull(TEXT("FPRItemInstance struct is available to inventory"), ItemInstanceStruct);

	if (!InventoryComponentClass || !ItemInstanceStruct)
	{
		return false;
	}

	TestNotNull(TEXT("CanAddItem function exists"), InventoryComponentClass->FindFunctionByName(TEXT("CanAddItem")));
	TestNotNull(TEXT("AddItem function exists"), InventoryComponentClass->FindFunctionByName(TEXT("AddItem")));
	TestNotNull(TEXT("RemoveItem function exists"), InventoryComponentClass->FindFunctionByName(TEXT("RemoveItem")));
	TestNotNull(TEXT("UseItem function exists"), InventoryComponentClass->FindFunctionByName(TEXT("UseItem")));
	TestNotNull(TEXT("GetItemCount function exists"), InventoryComponentClass->FindFunctionByName(TEXT("GetItemCount")));
	TestNotNull(TEXT("GetInventoryItems function exists for UI/debug"), InventoryComponentClass->FindFunctionByName(TEXT("GetInventoryItems")));
	TestNotNull(TEXT("SetItemDataAssets function exists for data-driven stacking"), InventoryComponentClass->FindFunctionByName(TEXT("SetItemDataAssets")));
	TestNotNull(TEXT("FindItemData function exists for UI and consumables"), InventoryComponentClass->FindFunctionByName(TEXT("FindItemData")));
	TestNotNull(TEXT("GetMaxStackCount function exists for data-driven stacking"), InventoryComponentClass->FindFunctionByName(TEXT("GetMaxStackCount")));
	TestNotNull(TEXT("Inventory exposes MaxSlots"), FindFProperty<FIntProperty>(InventoryComponentClass, TEXT("MaxSlots")));

	UScriptStruct* InventoryEntryStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRInventoryEntry"));
	UScriptStruct* InventoryListStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ProjectA.PRInventoryList"));
	TestNotNull(TEXT("FPRInventoryEntry struct exists"), InventoryEntryStruct);
	TestNotNull(TEXT("FPRInventoryList struct exists"), InventoryListStruct);
	TestTrue(
		TEXT("FPRInventoryEntry derives from FFastArraySerializerItem"),
		InventoryEntryStruct && InventoryEntryStruct->IsChildOf(FFastArraySerializerItem::StaticStruct()));
	TestTrue(
		TEXT("FPRInventoryList derives from FFastArraySerializer"),
		InventoryListStruct && InventoryListStruct->IsChildOf(FFastArraySerializer::StaticStruct()));

	FStructProperty* EntryItemProperty = InventoryEntryStruct ? FindFProperty<FStructProperty>(InventoryEntryStruct, TEXT("Item")) : nullptr;
	TestNotNull(TEXT("FPRInventoryEntry stores an FPRItemInstance Item"), EntryItemProperty);
	TestTrue(
		TEXT("FPRInventoryEntry Item property uses FPRItemInstance"),
		EntryItemProperty && EntryItemProperty->Struct == ItemInstanceStruct);

	FArrayProperty* EntriesProperty = InventoryListStruct ? FindFProperty<FArrayProperty>(InventoryListStruct, TEXT("Entries")) : nullptr;
	TestNotNull(TEXT("FPRInventoryList stores replicated Entries"), EntriesProperty);
	FStructProperty* EntriesInnerProperty = EntriesProperty ? CastField<FStructProperty>(EntriesProperty->Inner) : nullptr;
	TestTrue(
		TEXT("FPRInventoryList Entries contain FPRInventoryEntry"),
		EntriesInnerProperty && EntriesInnerProperty->Struct == InventoryEntryStruct);

	FStructProperty* InventoryListProperty = FindFProperty<FStructProperty>(InventoryComponentClass, TEXT("InventoryList"));
	TestNotNull(TEXT("Inventory component stores FPRInventoryList"), InventoryListProperty);
	TestTrue(
		TEXT("InventoryList property uses FPRInventoryList"),
		InventoryListProperty && InventoryListProperty->Struct == InventoryListStruct);
	TestTrue(
		TEXT("InventoryList is marked replicated"),
		InventoryListProperty && InventoryListProperty->HasAnyPropertyFlags(CPF_Net));
	TestNull(TEXT("Inventory no longer exposes a replicated raw Items array"), FindFProperty<FArrayProperty>(InventoryComponentClass, TEXT("Items")));
	TestNotNull(TEXT("Inventory exposes OnInventoryChanged for UI refresh"), FindFProperty<FMulticastDelegateProperty>(InventoryComponentClass, TEXT("OnInventoryChanged")));

	UActorComponent* InventoryComponentCDO = Cast<UActorComponent>(InventoryComponentClass->GetDefaultObject());
	TestTrue(
		TEXT("Inventory component replicates by default"),
		InventoryComponentCDO && InventoryComponentCDO->GetIsReplicated());
	TestEqual(
		TEXT("Inventory component replicates only to owning client"),
		InventoryComponentCDO ? InventoryComponentCDO->GetReplicationCondition() : COND_Never,
		COND_OwnerOnly);

	APRPlayerState* PlayerState = NewObject<APRPlayerState>();
	UObject* PlayerInventory = FindObject<UObject>(PlayerState, TEXT("InventoryComponent"));
	TestNotNull(TEXT("APRPlayerState owns InventoryComponent subobject"), PlayerInventory);
	TestTrue(
		TEXT("APRPlayerState InventoryComponent uses ProjectRift inventory class"),
		PlayerInventory && PlayerInventory->IsA(InventoryComponentClass));
	TestNotNull(
		TEXT("APRPlayerState exposes Blueprint inventory getter"),
		APRPlayerState::StaticClass()->FindFunctionByName(TEXT("GetInventoryComponent")));

	UObject* Inventory = NewObject<UObject>(GetTransientPackage(), InventoryComponentClass);
	TestNotNull(TEXT("Can instantiate UPRInventoryComponent"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	SetIntProperty(Inventory, TEXT("MaxSlots"), 1);

	TestTrue(TEXT("Can add valid item to empty inventory"), CallItemFunction(*this, Inventory, TEXT("CanAddItem"), TEXT("HealthInjector"), 2));
	TestTrue(TEXT("AddItem accepts valid item"), CallItemFunction(*this, Inventory, TEXT("AddItem"), TEXT("HealthInjector"), 2));
	TestEqual(TEXT("AddItem stores item count"), CallGetItemCount(*this, Inventory, TEXT("HealthInjector")), 2);

	TestTrue(TEXT("AddItem stacks matching ItemId"), CallItemFunction(*this, Inventory, TEXT("AddItem"), TEXT("HealthInjector"), 3));
	TestEqual(TEXT("Stacked item count is accumulated"), CallGetItemCount(*this, Inventory, TEXT("HealthInjector")), 5);

	TestFalse(TEXT("Inventory rejects new stack when full"), CallItemFunction(*this, Inventory, TEXT("CanAddItem"), TEXT("ShieldPack"), 1));
	TestFalse(TEXT("AddItem fails for new stack when full"), CallItemFunction(*this, Inventory, TEXT("AddItem"), TEXT("ShieldPack"), 1));
	TestEqual(TEXT("Rejected item was not added"), CallGetItemCount(*this, Inventory, TEXT("ShieldPack")), 0);

	TestTrue(TEXT("RemoveItem removes partial count"), CallNamedCountFunction(*this, Inventory, TEXT("RemoveItem"), TEXT("HealthInjector"), 2));
	TestEqual(TEXT("Partial remove decreases count"), CallGetItemCount(*this, Inventory, TEXT("HealthInjector")), 3);
	TestFalse(TEXT("RemoveItem rejects more than available count"), CallNamedCountFunction(*this, Inventory, TEXT("RemoveItem"), TEXT("HealthInjector"), 4));
	TestEqual(TEXT("Failed remove does not change count"), CallGetItemCount(*this, Inventory, TEXT("HealthInjector")), 3);
	TestTrue(TEXT("RemoveItem removes final stack"), CallNamedCountFunction(*this, Inventory, TEXT("RemoveItem"), TEXT("HealthInjector"), 3));
	TestEqual(TEXT("Final remove deletes stack"), CallGetItemCount(*this, Inventory, TEXT("HealthInjector")), 0);

	TestTrue(TEXT("UseItem consumes one item"), CallItemFunction(*this, Inventory, TEXT("AddItem"), TEXT("ShieldPack"), 1)
		&& CallUseItem(*this, Inventory, TEXT("ShieldPack")));
	TestEqual(TEXT("UseItem removes consumed item"), CallGetItemCount(*this, Inventory, TEXT("ShieldPack")), 0);
	TestFalse(TEXT("UseItem fails when item is missing"), CallUseItem(*this, Inventory, TEXT("ShieldPack")));

	UPRInventoryComponent* TypedInventory = NewObject<UPRInventoryComponent>(GetTransientPackage());
	TestNotNull(TEXT("Can instantiate typed inventory component"), TypedInventory);
	if (!TypedInventory)
	{
		return false;
	}

	SetIntProperty(TypedInventory, TEXT("MaxSlots"), 2);
	TestTrue(
		TEXT("Can add first affixed item instance"),
		TypedInventory->AddItem(MakeInventoryComponentTestItem(TEXT("ModdedChip"), 1, 1, EPRItemRarity::Rare, 1.0f, { TEXT("Burning") })));
	TestTrue(
		TEXT("Can add same ItemId with different affix as a separate instance"),
		TypedInventory->AddItem(MakeInventoryComponentTestItem(TEXT("ModdedChip"), 1, 1, EPRItemRarity::Rare, 1.0f, { TEXT("Freezing") })));
	TestEqual(TEXT("Different item instance data does not collapse into one stack"), TypedInventory->GetInventoryItems().Num(), 2);
	TestEqual(TEXT("GetItemCount sums all same-ItemId stacks"), TypedInventory->GetItemCount(TEXT("ModdedChip")), 2);

	UPRItemDataAsset* HealthInjectorData = NewObject<UPRItemDataAsset>(GetTransientPackage());
	HealthInjectorData->ItemId = TEXT("HealthInjector");
	HealthInjectorData->MaxStackCount = 5;

	UPRInventoryComponent* StackLimitInventory = NewObject<UPRInventoryComponent>(GetTransientPackage());
	TestNotNull(TEXT("Can instantiate stack limit inventory component"), StackLimitInventory);
	if (!StackLimitInventory)
	{
		return false;
	}

	SetIntProperty(StackLimitInventory, TEXT("MaxSlots"), 2);
	TestTrue(TEXT("Can register item data assets on inventory"), CallSetItemDataAssets(*this, StackLimitInventory, { HealthInjectorData }));
	TestEqual(TEXT("Inventory resolves data-driven max stack count"), CallGetMaxStackCount(*this, StackLimitInventory, TEXT("HealthInjector")), 5);
	TestTrue(TEXT("Can add more than one stack worth of stackable items"), StackLimitInventory->AddItem(MakeInventoryComponentTestItem(TEXT("HealthInjector"), 7)));
	TestEqual(TEXT("Stack-limited add keeps total count"), StackLimitInventory->GetItemCount(TEXT("HealthInjector")), 7);
	TestEqual(TEXT("Stack-limited add splits across slots"), StackLimitInventory->GetInventoryItems().Num(), 2);
	TestFalse(TEXT("Inventory rejects stackable add that cannot fully fit"), StackLimitInventory->CanAddItem(MakeInventoryComponentTestItem(TEXT("HealthInjector"), 4)));

	return true;
}

#endif
