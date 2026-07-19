#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Items/PRInventoryComponent.h"

namespace
{
FPRItemInstance MakeRuntimeIdentityItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Level = 1;
	Item.Rarity = EPRItemRarity::Common;
	Item.Durability = 1.0f;
	return Item;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemInstanceRuntimeTest,
	"ProjectRift.Items.InstanceIdentity.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemInstanceRuntimeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UPRInventoryComponent> Inventory(NewObject<UPRInventoryComponent>(GetTransientPackage()));
	const FPRItemInstance FirstIncoming = MakeRuntimeIdentityItem(TEXT("RuntimeIdentityChip"), 2);
	TestFalse(TEXT("Ingress template starts without a runtime identity"), FirstIncoming.HasValidIdentity());
	TestTrue(TEXT("Server inventory accepts the first stack"), Inventory->AddItem(FirstIncoming));

	const TArray<FPRItemInstance> FirstItems = Inventory->GetInventoryItems();
	TestEqual(TEXT("First ingress creates one stack"), FirstItems.Num(), 1);
	if (FirstItems.Num() != 1)
	{
		return false;
	}
	const FGuid FirstGuid = FirstItems[0].InstanceGuid;
	TestTrue(TEXT("First stack receives a server identity"), FirstGuid.IsValid());

	const FPRItemInstance SecondIncoming = MakeRuntimeIdentityItem(TEXT("RuntimeIdentityChip"), 3);
	TestTrue(TEXT("Server inventory accepts a compatible second stack"), Inventory->AddItem(SecondIncoming));
	const TArray<FPRItemInstance> MergedItems = Inventory->GetInventoryItems();
	TestEqual(TEXT("Compatible ingress merges into one stack"), MergedItems.Num(), 1);
	if (MergedItems.Num() == 1)
	{
		TestEqual(TEXT("Merged stack keeps its original identity"), MergedItems[0].InstanceGuid, FirstGuid);
		TestEqual(TEXT("Merged stack has the combined count"), MergedItems[0].Count, 5);
	}

	return true;
}

#endif
