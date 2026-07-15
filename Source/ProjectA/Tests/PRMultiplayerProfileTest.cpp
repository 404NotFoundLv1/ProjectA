#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Weapons/PRWeaponComponent.h"

namespace
{
FPRItemInstance MakeMultiplayerItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMultiplayerProfileContractTest,
	"ProjectRift.MultiplayerProfile.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMultiplayerProfileContractTest::RunTest(const FString& Parameters)
{
	FPRMultiplayerProfileProjection Projection;
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Client Profile");
	Projection.BackpackItems = { MakeMultiplayerItem(TEXT("HealthInjector"), 2) };
	Projection.Equipment = { FPRProfileEquipmentEntry(TEXT("Slot.Primary"), MakeMultiplayerItem(TEXT("HealthInjector"), 1)) };
	Projection.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 5 } };
	Projection.SelectedRoleId = TEXT("Ability.Role.Assault");
	Projection.Story.UnlockedChapterIds = { TEXT("Chapter.Prologue") };
	Projection.ShipModules = { FPRProfileShipModuleState{ TEXT("Ship.Module.Engine"), 0, false } };

	FString Diagnostic;
	TestTrue(TEXT("Valid multiplayer projection is accepted"), Projection.IsValid(&Diagnostic));
	FPRMultiplayerProfileProjection DuplicateEquipmentProjection = Projection;
	const FPRProfileEquipmentEntry DuplicateEquipment = DuplicateEquipmentProjection.Equipment[0];
	DuplicateEquipmentProjection.Equipment.Add(DuplicateEquipment);
	TestFalse(TEXT("Projection rejects duplicate equipment slots"), DuplicateEquipmentProjection.IsValid(&Diagnostic));
	FPRMultiplayerProfileProjection DuplicateStoryProjection = Projection;
	DuplicateStoryProjection.Story.CompletedStoryNodeIds = { TEXT("Story.Prologue.Intro"), TEXT("Story.Prologue.Intro") };
	TestFalse(TEXT("Projection rejects duplicate story nodes"), DuplicateStoryProjection.IsValid(&Diagnostic));
	FPRMultiplayerProfileProjection DuplicateModuleProjection = Projection;
	const FPRProfileShipModuleState DuplicateModule = DuplicateModuleProjection.ShipModules[0];
	DuplicateModuleProjection.ShipModules.Add(DuplicateModule);
	TestFalse(TEXT("Projection rejects duplicate ship modules"), DuplicateModuleProjection.IsValid(&Diagnostic));
	FPRMultiplayerProfileProjection InvalidModuleProjection = Projection;
	InvalidModuleProjection.ShipModules[0].Level = -1;
	TestFalse(TEXT("Projection rejects negative ship module levels"), InvalidModuleProjection.IsValid(&Diagnostic));
	Projection.ProfileId.Invalidate();
	TestFalse(TEXT("Projection rejects an invalid profile id"), Projection.IsValid(&Diagnostic));

	FPRPlayerSettlementReceipt Receipt;
	Receipt.ProfileId = FGuid::NewGuid();
	Receipt.RunId = FGuid::NewGuid();
	Receipt.SettlementId = FGuid::NewGuid();
	Receipt.MissionId = TEXT("Mission.Rift.Test.Hold");
	Receipt.Result = EPRRiftMissionResult::Success;
	Receipt.bExtracted = true;
	Receipt.bGrantStoryProgress = true;
	Receipt.SettledBackpackItems = { MakeMultiplayerItem(TEXT("ShieldPack"), 1) };
	Receipt.SettledEquipment = { FPRProfileEquipmentEntry(TEXT("Slot.Primary"), MakeMultiplayerItem(TEXT("TestRifle"), 1)) };
	Receipt.SettledResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 12 } };
	Receipt.SettledRoleId = TEXT("Ability.Role.Assault");
	TestTrue(TEXT("Valid personal settlement receipt is accepted"), Receipt.IsValid(&Diagnostic));
	FPRPlayerSettlementReceipt DuplicateReceiptEquipment = Receipt;
	const FPRProfileEquipmentEntry DuplicateSettledEquipmentEntry = DuplicateReceiptEquipment.SettledEquipment[0];
	DuplicateReceiptEquipment.SettledEquipment.Add(DuplicateSettledEquipmentEntry);
	TestFalse(TEXT("Receipt rejects duplicate equipment slots"), DuplicateReceiptEquipment.IsValid(&Diagnostic));

	FPRPlayerSettlementReceipt WrongReceipt = Receipt;
	WrongReceipt.SettlementId.Invalidate();
	TestFalse(TEXT("Receipt rejects an invalid settlement id"), WrongReceipt.IsValid(&Diagnostic));
	FPRPlayerSettlementReceipt FailedReceipt = Receipt;
	FailedReceipt.Result = EPRRiftMissionResult::Failed;
	FailedReceipt.bExtracted = false;
	FailedReceipt.bGrantStoryProgress = false;
	TestTrue(TEXT("Failure receipt without story advancement remains valid"), FailedReceipt.IsValid(&Diagnostic));
	FailedReceipt.bGrantStoryProgress = true;
	TestFalse(TEXT("Failure receipt cannot grant story advancement"), FailedReceipt.IsValid(&Diagnostic));

	const TArray<FPRItemInstance> Baseline = {
		MakeMultiplayerItem(TEXT("HealthInjector"), 3),
		MakeMultiplayerItem(TEXT("ShieldPack"), 1)
	};
	const TArray<FPRItemInstance> Runtime = {
		MakeMultiplayerItem(TEXT("HealthInjector"), 1),
		MakeMultiplayerItem(TEXT("ShieldPack"), 2),
		MakeMultiplayerItem(TEXT("LootOnly"), 4)
	};
	const TArray<FPRItemInstance> NonExtracted = FPRMultiplayerSettlementPolicy::BuildNonExtractedInventory(Baseline, Runtime);
	TestEqual(TEXT("Consumed baseline items remain consumed"), FPRMultiplayerSettlementPolicy::GetItemCount(NonExtracted, TEXT("HealthInjector")), 1);
	TestEqual(TEXT("Added copies are capped at the baseline"), FPRMultiplayerSettlementPolicy::GetItemCount(NonExtracted, TEXT("ShieldPack")), 1);
	TestEqual(TEXT("New loot is removed"), FPRMultiplayerSettlementPolicy::GetItemCount(NonExtracted, TEXT("LootOnly")), 0);
	const TArray<FPRProfileResourceBalance> NonExtractedWallet = FPRMultiplayerSettlementPolicy::BuildNonExtractedResourceWallet(
		{ FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 10 } },
		{ FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 7 }, FPRProfileResourceBalance{ TEXT("LootCurrency"), 5 } });
	TestEqual(TEXT("Non-extracted wallet preserves actual baseline consumption"), NonExtractedWallet[0].Count, 7);
	TestEqual(TEXT("Non-extracted wallet removes resources introduced during the run"), NonExtractedWallet.Num(), 1);

	APRPlayerState* LobbyState = NewObject<APRPlayerState>();
	APRPlayerState* RiftState = NewObject<APRPlayerState>();
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Travel Profile");
	Projection.BackpackItems = { MakeMultiplayerItem(TEXT("HealthInjector"), 2) };
	TestTrue(TEXT("Travel source accepts its profile projection"), LobbyState->BindMultiplayerProfile(Projection, Diagnostic));
	TestTrue(TEXT("Repeated binding of the same profile is accepted idempotently"), LobbyState->BindMultiplayerProfile(Projection, Diagnostic));
	TestEqual(TEXT("Repeated profile binding does not duplicate starter ammo"), LobbyState->GetInventoryComponent()->GetItemCount(TEXT("RifleAmmo")), 60);
	TestEqual(TEXT("Repeated profile binding does not duplicate the backpack starter rifle"), LobbyState->GetInventoryComponent()->GetItemCount(TEXT("TestRifle")), 1);
	LobbyState->CopyProperties(RiftState);
	TestTrue(TEXT("Profile binding survives seamless PlayerState replacement"), RiftState->IsMultiplayerProfileBound());
	TestEqual(TEXT("Bound profile id survives seamless PlayerState replacement"), RiftState->GetBoundProfileId(), Projection.ProfileId);
	TestEqual(TEXT("Mission baseline includes the preserved bag plus starter rifle and ammo"), RiftState->GetMissionStartBackpackItems().Num(), 3);
	TestEqual(TEXT("Runtime inventory survives seamless PlayerState replacement"), RiftState->GetInventoryComponent()->GetItemCount(TEXT("HealthInjector")), 2);
	TestEqual(TEXT("Starter rifle remains in backpack beside unknown legacy primary"), RiftState->GetInventoryComponent()->GetItemCount(TEXT("TestRifle")), 1);
	TestEqual(TEXT("Starter total ammo survives seamless PlayerState replacement"), RiftState->GetInventoryComponent()->GetItemCount(TEXT("RifleAmmo")), 60);
	TestEqual(TEXT("Bound equipment is retained on lobby PlayerState"), LobbyState->GetWeaponComponent()->GetEquipmentEntries().Num(), 1);
	TestEqual(TEXT("Bound equipment survives seamless PlayerState replacement"), RiftState->GetWeaponComponent()->GetEquipmentEntries().Num(), 1);
	TestEqual(TEXT("Unknown legacy primary is preserved without replacement"), RiftState->GetWeaponComponent()->GetEquipmentEntries()[0].Item.ItemId, FName(TEXT("HealthInjector")));
	TestEqual(TEXT("Bound ship modules are stored by PlayerState"), LobbyState->GetBoundShipModules().Num(), 1);
	TestEqual(TEXT("Bound ship modules survive seamless PlayerState replacement"), RiftState->GetBoundShipModules().Num(), 1);

	APRPlayerState* StarterLobbyState = NewObject<APRPlayerState>();
	APRPlayerState* StarterReturnState = NewObject<APRPlayerState>();
	FPRMultiplayerProfileProjection StarterProjection;
	StarterProjection.ProfileId = FGuid::NewGuid();
	StarterProjection.DisplayName = TEXT("Starter Travel Profile");
	TestTrue(TEXT("Empty travel profile receives the equipped starter rifle"), StarterLobbyState->BindMultiplayerProfile(StarterProjection, Diagnostic));
	StarterLobbyState->CopyProperties(StarterReturnState);
	StarterProjection.BackpackItems = { MakeMultiplayerItem(TEXT("RifleAmmo"), 48) };
	TestTrue(TEXT("Returning seamless PlayerState accepts the same stale profile projection"), StarterReturnState->BindMultiplayerProfile(StarterProjection, Diagnostic));
	TestEqual(TEXT("Stale same-profile rebind preserves one starter magazine"), StarterReturnState->GetWeaponComponent()->GetMagazineAmmo(), 12);
	TestEqual(TEXT("Stale same-profile rebind does not duplicate starter reserve"), StarterReturnState->GetInventoryComponent()->GetItemCount(TEXT("RifleAmmo")), 48);

	const FGuid RepairTransactionId = FGuid::NewGuid();
	LobbyState->SetRepairPersistencePending(RepairTransactionId, true);
	LobbyState->SetReady(true);
	TestFalse(TEXT("Pending repair persistence blocks ready state"), LobbyState->IsReady());
	APRPlayerState* PendingTravelState = NewObject<APRPlayerState>();
	LobbyState->CopyProperties(PendingTravelState);
	TestTrue(TEXT("Pending repair state survives seamless PlayerState replacement"), PendingTravelState->IsRepairPersistencePending());
	TestEqual(TEXT("Pending repair transaction survives seamless PlayerState replacement"), PendingTravelState->GetPendingRepairTransactionId(), RepairTransactionId);
	LobbyState->SetRepairPersistencePending(RepairTransactionId, false);
	LobbyState->SetReady(true);
	TestTrue(TEXT("Successful repair acknowledgement restores ready eligibility"), LobbyState->IsReady());

	FPRShipRepairReceipt RepairReceipt;
	RepairReceipt.ProfileId = Projection.ProfileId;
	RepairReceipt.RepairProjectId = TEXT("Repair.Ship.Engine.Stage1");
	RepairReceipt.TransactionId = RepairTransactionId;
	RepairReceipt.SettledShipModules = { FPRProfileShipModuleState{ TEXT("Ship.Module.Engine"), 1, true } };
	RepairReceipt.SettledStory.UnlockedChapterIds = { TEXT("Chapter.One"), TEXT("Chapter.Prologue") };
	RepairReceipt.SettledStory.CurrentChapterId = TEXT("Chapter.One");
	TestTrue(TEXT("Server-shaped repair state applies to bound PlayerState"), LobbyState->ApplyShipRepairState(RepairReceipt, Diagnostic));
	FPRProfileSnapshot ServerSnapshot;
	TestTrue(TEXT("PlayerState can rebuild its authoritative repair snapshot"), LobbyState->BuildBoundShipRepairSnapshot(ServerSnapshot, Diagnostic));
	TestEqual(TEXT("Authoritative repair snapshot keeps the repaired module"), ServerSnapshot.ShipModules.Num(), 1);
	TestEqual(TEXT("Authoritative repair snapshot keeps the repaired chapter"), ServerSnapshot.Story.CurrentChapterId, FName(TEXT("Chapter.One")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMultiplayerProfileRpcContractTest,
	"ProjectRift.MultiplayerProfile.RpcContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMultiplayerProfileRpcContractTest::RunTest(const FString& Parameters)
{
	const UScriptStruct* ProjectionStruct = FPRMultiplayerProfileProjection::StaticStruct();
	TestNotNull(TEXT("Multiplayer projection exposes ship modules"), ProjectionStruct->FindPropertyByName(TEXT("ShipModules")));

	const UClass* ControllerClass = APRPlayerController::StaticClass();
	const UFunction* BindFunction = ControllerClass->FindFunctionByName(TEXT("ServerBindMultiplayerProfile"));
	const UFunction* ReceiptFunction = ControllerClass->FindFunctionByName(TEXT("ClientReceivePersonalSettlement"));
	const UFunction* AckFunction = ControllerClass->FindFunctionByName(TEXT("ServerAcknowledgePersonalSettlement"));
	const UFunction* PendingFunction = ControllerClass->FindFunctionByName(TEXT("ServerReportPendingSettlementSave"));
	const UFunction* RepairRequestFunction = ControllerClass->FindFunctionByName(TEXT("ServerRequestShipRepair"));
	const UFunction* RepairReceiptFunction = ControllerClass->FindFunctionByName(TEXT("ClientReceiveShipRepairReceipt"));
	const UFunction* RepairAckFunction = ControllerClass->FindFunctionByName(TEXT("ServerAcknowledgeShipRepair"));
	const UFunction* RepairPendingFunction = ControllerClass->FindFunctionByName(TEXT("ServerReportPendingShipRepairSave"));
	TestTrue(TEXT("Profile binding uses a reliable server RPC"), BindFunction && BindFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Personal receipt uses a reliable client RPC"), ReceiptFunction && ReceiptFunction->HasAnyFunctionFlags(FUNC_NetClient | FUNC_NetReliable));
	TestTrue(TEXT("Settlement acknowledgement uses a reliable server RPC"), AckFunction && AckFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Pending settlement blocks lobby readiness through a reliable server RPC"), PendingFunction && PendingFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Ship repair request uses a reliable server RPC"), RepairRequestFunction && RepairRequestFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Ship repair receipt uses a reliable owner client RPC"), RepairReceiptFunction && RepairReceiptFunction->HasAnyFunctionFlags(FUNC_NetClient | FUNC_NetReliable));
	TestTrue(TEXT("Ship repair acknowledgement uses a reliable server RPC"), RepairAckFunction && RepairAckFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Pending ship repair blocks lobby readiness through a reliable server RPC"), RepairPendingFunction && RepairPendingFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));

	const UClass* PlayerStateClass = APRPlayerState::StaticClass();
	TestNotNull(TEXT("PlayerState stores the bound ship module projection"), PlayerStateClass->FindPropertyByName(TEXT("BoundShipModules")));
	TestNotNull(TEXT("PlayerState replicates pending repair persistence state"), PlayerStateClass->FindPropertyByName(TEXT("bRepairPersistencePending")));

	const UClass* SaveSubsystemClass = UPRSaveSubsystem::StaticClass();
	TestNotNull(TEXT("Save subsystem exposes multiplayer projection builder"), SaveSubsystemClass->FindFunctionByName(TEXT("BuildMultiplayerProfileProjection")));
	TestNotNull(TEXT("Save subsystem exposes transactional settlement apply"), SaveSubsystemClass->FindFunctionByName(TEXT("ApplyMultiplayerSettlementReceipt")));
	TestNotNull(TEXT("Save subsystem exposes transactional ship repair apply"), SaveSubsystemClass->FindFunctionByName(TEXT("ApplyShipRepairReceipt")));
	TestNotNull(TEXT("Save subsystem exposes pending ship repair retry"), SaveSubsystemClass->FindFunctionByName(TEXT("RetryPendingShipRepairReceipt")));
	return true;
}

#endif
