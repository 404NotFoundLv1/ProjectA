#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataAsset.h"
#include "GameFramework/DefaultPawn.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRLootTableLibrary.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRLootTableTest, "ProjectRift.Items.LootTable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
APRPlayerController* SpawnLootTableTestPlayer(FAutomationTestBase& Test, UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APawn* Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Loot table test controller spawned"), PlayerController);
	Test.TestNotNull(TEXT("Loot table test player state spawned"), PlayerState);
	Test.TestNotNull(TEXT("Loot table test pawn spawned"), Pawn);
	if (!PlayerController || !PlayerState || !Pawn)
	{
		return nullptr;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(Pawn);
	return PlayerController;
}

UPRInventoryComponent* GetLootTableTestInventory(const APRPlayerController* PlayerController)
{
	const APRPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APRPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}

bool RollAndTestItem(FAutomationTestBase& Test, const UPRLootTableDataAsset* LootTable, const float Roll, const FName ExpectedItemId)
{
	FPRItemInstance RolledItem;
	const bool bRolled = LootTable && LootTable->RollLoot(Roll, RolledItem);
	Test.TestTrue(FString::Printf(TEXT("Roll %.2f succeeds"), Roll), bRolled);
	Test.TestEqual(FString::Printf(TEXT("Roll %.2f returns expected item"), Roll), RolledItem.ItemId, ExpectedItemId);
	return bRolled && RolledItem.ItemId == ExpectedItemId;
}
}

bool FPRLootTableTest::RunTest(const FString& Parameters)
{
	UClass* LootTableClass = UPRLootTableDataAsset::StaticClass();
	TestTrue(TEXT("UPRLootTableDataAsset derives from UDataAsset"), LootTableClass->IsChildOf(UDataAsset::StaticClass()));
	TestNotNull(TEXT("Loot table exposes entries"), FindFProperty<FArrayProperty>(LootTableClass, TEXT("Entries")));
	TestNotNull(TEXT("Loot table exposes IsValidLootTable"), LootTableClass->FindFunctionByName(TEXT("IsValidLootTable")));
	TestNotNull(TEXT("Loot table exposes GetTotalWeight"), LootTableClass->FindFunctionByName(TEXT("GetTotalWeight")));
	TestNotNull(TEXT("Loot table exposes RollLoot"), LootTableClass->FindFunctionByName(TEXT("RollLoot")));

	UClass* LootTableLibraryClass = UPRLootTableLibrary::StaticClass();
	TestNotNull(TEXT("Loot table library exposes SpawnLootPickupFromTable"), LootTableLibraryClass->FindFunctionByName(TEXT("SpawnLootPickupFromTable")));
	TestNotNull(TEXT("Loot table library exposes SpawnSeededLootPickupFromTable"), LootTableLibraryClass->FindFunctionByName(TEXT("SpawnSeededLootPickupFromTable")));
	UFunction* SpawnLootFunction = LootTableLibraryClass->FindFunctionByName(TEXT("SpawnLootPickupFromTable"));
	TestTrue(
		TEXT("Loot table pickup spawning is marked BlueprintAuthorityOnly"),
		SpawnLootFunction && SpawnLootFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Player controller exposes SpawnTestLoot"), PlayerControllerClass->FindFunctionByName(TEXT("SpawnTestLoot")));
	UFunction* ServerSpawnTestLootFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerSpawnTestLoot"));
	TestNotNull(TEXT("Player controller exposes ServerSpawnTestLoot"), ServerSpawnTestLootFunction);
	TestTrue(TEXT("ServerSpawnTestLoot is a server RPC"), ServerSpawnTestLootFunction && ServerSpawnTestLootFunction->HasAnyFunctionFlags(FUNC_NetServer));

	UPRLootTableDataAsset* LootTable = NewObject<UPRLootTableDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Can instantiate default test loot table"), LootTable);
	if (!LootTable)
	{
		return false;
	}

	TestTrue(TEXT("Default test loot table is valid"), LootTable->IsValidLootTable());
	TestEqual(TEXT("Default test loot table has four entries"), LootTable->Entries.Num(), 4);
	TestEqual(TEXT("Default test loot table total weight is 100"), LootTable->GetTotalWeight(), 100.0f);

	if (LootTable->Entries.Num() == 4)
	{
		TestEqual(TEXT("HealthInjector weight is 40"), LootTable->Entries[0].Weight, 40.0f);
		TestEqual(TEXT("ShieldPack weight is 30"), LootTable->Entries[1].Weight, 30.0f);
		TestEqual(TEXT("EnergyCrystal weight is 20"), LootTable->Entries[2].Weight, 20.0f);
		TestEqual(TEXT("Test chip weight is 10"), LootTable->Entries[3].Weight, 10.0f);
		TestTrue(TEXT("Test chip entry enables deterministic equipment affixes"), LootTable->Entries[3].bGenerateEquipmentAffixes);
	}

	RollAndTestItem(*this, LootTable, 0.0f, TEXT("HealthInjector"));
	RollAndTestItem(*this, LootTable, 39.99f, TEXT("HealthInjector"));
	RollAndTestItem(*this, LootTable, 40.0f, TEXT("ShieldPack"));
	RollAndTestItem(*this, LootTable, 69.99f, TEXT("ShieldPack"));
	RollAndTestItem(*this, LootTable, 70.0f, TEXT("EnergyCrystal"));
	RollAndTestItem(*this, LootTable, 89.99f, TEXT("EnergyCrystal"));
	RollAndTestItem(*this, LootTable, 90.0f, TEXT("DA_TestChip"));
	RollAndTestItem(*this, LootTable, 100.0f, TEXT("DA_TestChip"));

	int32 EquipmentSeed = INDEX_NONE;
	FPRItemInstance SeededEquipment;
	for (int32 CandidateSeed = 0; CandidateSeed < 1024; ++CandidateSeed)
	{
		FPRItemInstance Candidate;
		FString Diagnostic;
		if (LootTable->RollSeededLoot(CandidateSeed, Candidate, Diagnostic) && Candidate.AffixGenerationVersion == 1)
		{
			EquipmentSeed = CandidateSeed;
			SeededEquipment = Candidate;
			break;
		}
	}
	TestTrue(TEXT("A server seed can select and generate the equipment loot entry"), EquipmentSeed != INDEX_NONE);
	if (EquipmentSeed != INDEX_NONE)
	{
		FPRItemInstance RepeatedSeededEquipment;
		FString Diagnostic;
		TestTrue(TEXT("The same server seed rerolls successfully"), LootTable->RollSeededLoot(EquipmentSeed, RepeatedSeededEquipment, Diagnostic));
		TestTrue(TEXT("The same server seed preserves the final generated instance"), SeededEquipment.HasEquivalentStackingState(RepeatedSeededEquipment));
		TestEqual(TEXT("Generated equipment persists its server LootSeed"), SeededEquipment.LootSeed, EquipmentSeed);
	}

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	const FVector DeathLocation(320.0f, 120.0f, 40.0f);
	APRPickupActor* SpawnedLoot = UPRLootTableLibrary::SpawnLootPickupFromTable(
		World,
		LootTable,
		APRPickupActor::StaticClass(),
		DeathLocation,
		FRotator::ZeroRotator,
		70.0f);

	TestNotNull(TEXT("Loot table spawns pickup actor at death location"), SpawnedLoot);
	if (!SpawnedLoot)
	{
		return false;
	}

	TestTrue(TEXT("Spawned loot pickup replicates"), SpawnedLoot->GetIsReplicated());
	TestTrue(TEXT("Spawned loot is available"), SpawnedLoot->CanBePickedUp());
	TestEqual(TEXT("Spawned loot is EnergyCrystal"), SpawnedLoot->GetItemInstance().ItemId, FName(TEXT("EnergyCrystal")));
	TestEqual(TEXT("Spawned loot count is one"), SpawnedLoot->GetItemInstance().Count, 1);
	TestTrue(TEXT("Spawned loot is near death location"), FVector::DistSquared(SpawnedLoot->GetActorLocation(), DeathLocation) < FMath::Square(20.0f));
	if (EquipmentSeed != INDEX_NONE)
	{
		APRPickupActor* SeededPickup = UPRLootTableLibrary::SpawnSeededLootPickupFromTable(
			World,
			LootTable,
			APRPickupActor::StaticClass(),
			DeathLocation + FVector(80.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			EquipmentSeed);
		TestNotNull(TEXT("Server seed spawns a generated equipment pickup"), SeededPickup);
		TestTrue(TEXT("Seeded pickup preserves the final deterministic item"), SeededPickup && SeededPickup->GetItemInstance().HasEquivalentStackingState(SeededEquipment));
	}

	APRPlayerController* Picker = SpawnLootTableTestPlayer(*this, World, DeathLocation);
	UPRInventoryComponent* PickerInventory = GetLootTableTestInventory(Picker);
	TestNotNull(TEXT("Picker inventory exists"), PickerInventory);
	TestTrue(TEXT("Player can pick up loot table drop"), Picker && Picker->TryPickupOnServer(SpawnedLoot));
	TestEqual(TEXT("Picked loot enters inventory"), PickerInventory ? PickerInventory->GetItemCount(TEXT("EnergyCrystal")) : INDEX_NONE, 1);

	APRPlayerController* DebugLootController = SpawnLootTableTestPlayer(*this, World, FVector(700.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("Debug loot controller spawned"), DebugLootController);
	APRPickupActor* DebugLoot = DebugLootController ? DebugLootController->TrySpawnTestLootOnServer(0.0f) : nullptr;
	TestNotNull(TEXT("Debug server loot spawn returns pickup"), DebugLoot);
	if (DebugLoot)
	{
		TestEqual(TEXT("Debug loot uses default table"), DebugLoot->GetItemInstance().ItemId, FName(TEXT("HealthInjector")));
		TestTrue(TEXT("Debug loot pickup replicates"), DebugLoot->GetIsReplicated());
	}

	return true;
}

#endif
