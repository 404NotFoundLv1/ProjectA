#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Items/PRItemTypes.h"
#include "Persistence/PRProfileSave.h"
#include "UObject/UnrealType.h"

namespace
{
FPRItemInstance MakeIdentityItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	Item.Level = 2;
	Item.Rarity = EPRItemRarity::Rare;
	Item.Durability = 0.8f;
	Item.Affixes = { TEXT("Affix.Identity") };
	return Item;
}

UPRProfileSave* MakeLegacyIdentityProfile(const FGuid& ProfileId)
{
	UPRProfileSave* Profile = NewObject<UPRProfileSave>(GetTransientPackage());
	Profile->ProfileId = ProfileId;
	Profile->DisplayName = TEXT("Identity Migration");
	Profile->CreatedUtc = FDateTime(2026, 7, 19, 0, 0, 0);
	Profile->LastSavedUtc = Profile->CreatedUtc;
	Profile->SaveVersion = 4;
	Profile->Snapshot.BackpackItems = { MakeIdentityItem(TEXT("HealthInjector"), 2) };
	Profile->Snapshot.WarehouseItems = { MakeIdentityItem(TEXT("ShieldPack"), 1) };
	Profile->Snapshot.Equipment = {
		FPRProfileEquipmentEntry(TEXT("Slot.Primary"), MakeIdentityItem(TEXT("TestRifle"), 1))
	};
	return Profile;
}

const FStructProperty* FindInstanceGuidProperty()
{
	return FindFProperty<FStructProperty>(FPRItemInstance::StaticStruct(), TEXT("InstanceGuid"));
}

bool ReadInstanceGuid(const FPRItemInstance& Item, FGuid& OutGuid)
{
	const FStructProperty* Property = FindInstanceGuidProperty();
	if (!Property || Property->Struct != TBaseStructure<FGuid>::Get())
	{
		return false;
	}
	OutGuid = *Property->ContainerPtrToValuePtr<FGuid>(&Item);
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRItemInstanceIdentityTest,
	"ProjectRift.Items.InstanceIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRItemInstanceIdentityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Player profile schema is v5"), UPRProfileSave::LatestSaveVersion, 5);

	const FStructProperty* InstanceGuidProperty = FindInstanceGuidProperty();
	TestNotNull(TEXT("Item instances expose InstanceGuid"), InstanceGuidProperty);
	if (InstanceGuidProperty)
	{
		TestTrue(TEXT("InstanceGuid uses FGuid"), InstanceGuidProperty->Struct == TBaseStructure<FGuid>::Get());
	}

	const FGuid LegacyProfileId = FGuid::NewGuid();
	UPRProfileSave* FirstMigration = MakeLegacyIdentityProfile(LegacyProfileId);
	UPRProfileSave* SecondMigration = MakeLegacyIdentityProfile(LegacyProfileId);
	const FPRProfileOperationResult FirstResult = FirstMigration->MigrateToLatest();
	const FPRProfileOperationResult SecondResult = SecondMigration->MigrateToLatest();
	TestTrue(TEXT("First v4 profile migrates to v5"), FirstResult.IsSuccess());
	TestTrue(TEXT("Second equivalent v4 profile migrates to v5"), SecondResult.IsSuccess());
	TestEqual(TEXT("First migration reaches v5"), FirstMigration->SaveVersion, 5);
	TestEqual(TEXT("Second migration reaches v5"), SecondMigration->SaveVersion, 5);

	FGuid FirstBackpackGuid;
	FGuid SecondBackpackGuid;
	const bool bReadFirst = FirstMigration->Snapshot.BackpackItems.Num() == 1
		&& ReadInstanceGuid(FirstMigration->Snapshot.BackpackItems[0], FirstBackpackGuid);
	const bool bReadSecond = SecondMigration->Snapshot.BackpackItems.Num() == 1
		&& ReadInstanceGuid(SecondMigration->Snapshot.BackpackItems[0], SecondBackpackGuid);
	TestTrue(TEXT("Migrated backpack item exposes a GUID"), bReadFirst);
	TestTrue(TEXT("Equivalent migration exposes a GUID"), bReadSecond);
	if (bReadFirst && bReadSecond)
	{
		TestTrue(TEXT("Migrated item GUID is valid"), FirstBackpackGuid.IsValid());
		TestEqual(TEXT("Migration is deterministic for the same profile"), FirstBackpackGuid, SecondBackpackGuid);
	}

	UPRProfileSave* IsolatedMigration = MakeLegacyIdentityProfile(FGuid::NewGuid());
	TestTrue(TEXT("Different profile migration succeeds"), IsolatedMigration->MigrateToLatest().IsSuccess());
	FGuid IsolatedBackpackGuid;
	const bool bReadIsolated = IsolatedMigration->Snapshot.BackpackItems.Num() == 1
		&& ReadInstanceGuid(IsolatedMigration->Snapshot.BackpackItems[0], IsolatedBackpackGuid);
	TestTrue(TEXT("Different profile migration exposes a GUID"), bReadIsolated);
	if (bReadFirst && bReadIsolated)
	{
		TestNotEqual(TEXT("Profile identity namespaces migrated GUIDs"), FirstBackpackGuid, IsolatedBackpackGuid);
	}

	UPRProfileSave* InvalidV5Profile = MakeLegacyIdentityProfile(FGuid::NewGuid());
	InvalidV5Profile->SaveVersion = 5;
	FGuid DuplicateGuid = FGuid::NewGuid();
	if (FStructProperty* MutableInstanceGuidProperty = FindFProperty<FStructProperty>(FPRItemInstance::StaticStruct(), TEXT("InstanceGuid")))
	{
		*MutableInstanceGuidProperty->ContainerPtrToValuePtr<FGuid>(&InvalidV5Profile->Snapshot.BackpackItems[0]) = DuplicateGuid;
		*MutableInstanceGuidProperty->ContainerPtrToValuePtr<FGuid>(&InvalidV5Profile->Snapshot.WarehouseItems[0]) = DuplicateGuid;
	}
	const FPRProfileOperationResult InvalidV5Result = InvalidV5Profile->MigrateToLatest();
	TestFalse(TEXT("v5 profile rejects duplicate instance GUIDs"), InvalidV5Result.IsSuccess());

	return true;
}

#endif
