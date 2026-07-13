#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Items/PRInventoryComponent.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Progression/PRMissionProgressionDataAsset.h"

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
	Projection.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 5 } };
	Projection.SelectedRoleId = TEXT("Ability.Role.Assault");
	Projection.Story.UnlockedChapterIds = { TEXT("Chapter.Prologue") };

	FString Diagnostic;
	TestTrue(TEXT("Valid multiplayer projection is accepted"), Projection.IsValid(&Diagnostic));
	FPRMultiplayerProfileProjection DuplicateStoryProjection = Projection;
	DuplicateStoryProjection.Story.CompletedStoryNodeIds = { TEXT("Story.Prologue.Intro"), TEXT("Story.Prologue.Intro") };
	TestFalse(TEXT("Projection rejects duplicate story nodes"), DuplicateStoryProjection.IsValid(&Diagnostic));
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
	Receipt.SettledResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 12 } };
	Receipt.SettledRoleId = TEXT("Ability.Role.Assault");
	TestTrue(TEXT("Valid personal settlement receipt is accepted"), Receipt.IsValid(&Diagnostic));

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
	LobbyState->CopyProperties(RiftState);
	TestTrue(TEXT("Profile binding survives seamless PlayerState replacement"), RiftState->IsMultiplayerProfileBound());
	TestEqual(TEXT("Bound profile id survives seamless PlayerState replacement"), RiftState->GetBoundProfileId(), Projection.ProfileId);
	TestEqual(TEXT("Mission baseline survives seamless PlayerState replacement"), RiftState->GetMissionStartBackpackItems().Num(), 1);
	TestEqual(TEXT("Runtime inventory survives seamless PlayerState replacement"), RiftState->GetInventoryComponent()->GetItemCount(TEXT("HealthInjector")), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMultiplayerProfileRpcContractTest,
	"ProjectRift.MultiplayerProfile.RpcContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMultiplayerProfileRpcContractTest::RunTest(const FString& Parameters)
{
	const UClass* ControllerClass = APRPlayerController::StaticClass();
	const UFunction* BindFunction = ControllerClass->FindFunctionByName(TEXT("ServerBindMultiplayerProfile"));
	const UFunction* ReceiptFunction = ControllerClass->FindFunctionByName(TEXT("ClientReceivePersonalSettlement"));
	const UFunction* AckFunction = ControllerClass->FindFunctionByName(TEXT("ServerAcknowledgePersonalSettlement"));
	const UFunction* PendingFunction = ControllerClass->FindFunctionByName(TEXT("ServerReportPendingSettlementSave"));
	TestTrue(TEXT("Profile binding uses a reliable server RPC"), BindFunction && BindFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Personal receipt uses a reliable client RPC"), ReceiptFunction && ReceiptFunction->HasAnyFunctionFlags(FUNC_NetClient | FUNC_NetReliable));
	TestTrue(TEXT("Settlement acknowledgement uses a reliable server RPC"), AckFunction && AckFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));
	TestTrue(TEXT("Pending settlement blocks lobby readiness through a reliable server RPC"), PendingFunction && PendingFunction->HasAnyFunctionFlags(FUNC_NetServer | FUNC_NetReliable));

	const UClass* SaveSubsystemClass = UPRSaveSubsystem::StaticClass();
	TestNotNull(TEXT("Save subsystem exposes multiplayer projection builder"), SaveSubsystemClass->FindFunctionByName(TEXT("BuildMultiplayerProfileProjection")));
	TestNotNull(TEXT("Save subsystem exposes transactional settlement apply"), SaveSubsystemClass->FindFunctionByName(TEXT("ApplyMultiplayerSettlementReceipt")));
	return true;
}

#endif
