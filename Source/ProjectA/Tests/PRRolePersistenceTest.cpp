#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Persistence/PRProfileRuntimeBridge.h"
#include "Persistence/PRProfileTypes.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleComponent.h"

namespace
{
TArray<FPRRoleModuleSlotEntry> MakeUnknownRoleLayout()
{
	return {
		FPRRoleModuleSlotEntry(EPRRoleModuleSlot::Q, TEXT("Ability.Module.Legacy.UnknownQ")),
		FPRRoleModuleSlotEntry(EPRRoleModuleSlot::E, TEXT("Ability.Module.Legacy.UnknownE")),
		FPRRoleModuleSlotEntry(EPRRoleModuleSlot::R, TEXT("Ability.Module.Legacy.UnknownR"))
	};
}

bool AreRoleLoadoutsEqual(const FPRRoleLoadout& Left, const FPRRoleLoadout& Right)
{
	if (Left.Entries.Num() != Right.Entries.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Entries.Num(); ++Index)
	{
		if (Left.Entries[Index].Slot != Right.Entries[Index].Slot
			|| Left.Entries[Index].ModuleId != Right.Entries[Index].ModuleId)
		{
			return false;
		}
	}
	return true;
}

bool AreItemArraysEqual(const TArray<FPRItemInstance>& Left, const TArray<FPRItemInstance>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		if (Left[Index].ItemId != Right[Index].ItemId
			|| Left[Index].Count != Right[Index].Count
			|| Left[Index].Level != Right[Index].Level
			|| Left[Index].Rarity != Right[Index].Rarity
			|| !FMath::IsNearlyEqual(Left[Index].Durability, Right[Index].Durability)
			|| Left[Index].Affixes != Right[Index].Affixes)
		{
			return false;
		}
	}
	return true;
}

bool AreResourceWalletsEqual(const TArray<FPRProfileResourceBalance>& Left, const TArray<FPRProfileResourceBalance>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		if (Left[Index].ResourceId != Right[Index].ResourceId || Left[Index].Count != Right[Index].Count)
		{
			return false;
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRolePersistenceTest,
	"ProjectRift.Roles.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRolePersistenceTest::RunTest(const FString& Parameters)
{
	const FName UnknownRoleId(TEXT("Ability.Role.Legacy.Unknown"));
	const TArray<FPRRoleModuleSlotEntry> UnknownLayout = MakeUnknownRoleLayout();

	FPRProfileSnapshot Snapshot;
	Snapshot.SelectedRoleId = UnknownRoleId;
	Snapshot.UnlockedRoleIds = { UnknownRoleId };
	Snapshot.UnlockedRoleModuleIds = {
		TEXT("Ability.Module.Legacy.UnknownQ"),
		TEXT("Ability.Module.Legacy.UnknownE"),
		TEXT("Ability.Module.Legacy.UnknownR")
	};
	Snapshot.EquippedRoleModules = UnknownLayout;
	Snapshot.Normalize();
	FString Diagnostic;
	TestTrue(TEXT("A v4 snapshot structurally preserves unknown role data"), Snapshot.IsValid(&Diagnostic));
	TestEqual(TEXT("Unknown permanent module unlocks survive normalization"), Snapshot.UnlockedRoleModuleIds.Num(), 3);
	TestEqual(TEXT("Unknown equipped Q/E/R entries survive normalization"), Snapshot.EquippedRoleModules.Num(), 3);
	TestEqual(TEXT("Unknown equipped Q module identity is unchanged"), Snapshot.EquippedRoleModules[0].ModuleId, UnknownLayout[0].ModuleId);

	FPRMultiplayerProfileProjection Projection;
	Projection.ProfileId = FGuid::NewGuid();
	Projection.DisplayName = TEXT("Unknown Role Persistence");
	Projection.SelectedRoleId = UnknownRoleId;
	Projection.UnlockedRoleIds = { UnknownRoleId };
	Projection.UnlockedRoleModuleIds = Snapshot.UnlockedRoleModuleIds;
	Projection.EquippedRoleModules = UnknownLayout;
	TestTrue(TEXT("Profile projection accepts structurally valid unknown role data"), Projection.IsValid(&Diagnostic));

	APRPlayerState* LobbyState = NewObject<APRPlayerState>();
	TestTrue(TEXT("Bound profile applies unknown role state without deleting it"), LobbyState->BindMultiplayerProfile(Projection, Diagnostic));
	UPRRoleComponent* RoleComponent = LobbyState->GetRoleComponent();
	TestNotNull(TEXT("Bound profile owns a role component"), RoleComponent);
	if (!RoleComponent)
	{
		return false;
	}
	TestEqual(TEXT("Unknown role remains the selected compatibility mirror"), LobbyState->GetSelectedRoleModule(), UnknownRoleId);
	TestEqual(TEXT("Unknown equipped layout reaches runtime unchanged"), RoleComponent->GetCurrentLoadout().Entries.Num(), 3);
	TestEqual(TEXT("Unknown module unlocks reach runtime unchanged"), RoleComponent->GetUnlockedModuleIds().Num(), 3);
	LobbyState->SetReady(true);
	TestFalse(TEXT("A bound unknown active role layout cannot become ready"), LobbyState->IsReady());

	FPRProfileSnapshot CapturedSnapshot;
	TestTrue(TEXT("Bound runtime state rebuilds a persistence snapshot"), LobbyState->BuildBoundShipRepairSnapshot(CapturedSnapshot, Diagnostic));
	TestEqual(TEXT("Authoritative snapshot keeps unknown module unlocks"), CapturedSnapshot.UnlockedRoleModuleIds, Snapshot.UnlockedRoleModuleIds);
	TestEqual(TEXT("Authoritative snapshot keeps unknown equipped entries"), CapturedSnapshot.EquippedRoleModules.Num(), UnknownLayout.Num());

	FPRProfileSnapshot LegacyAssaultSnapshot;
	LegacyAssaultSnapshot.SelectedRoleId = TEXT("Ability.Role.Assault");
	LegacyAssaultSnapshot.UnlockedRoleIds = { TEXT("Ability.Role.Assault") };
	LegacyAssaultSnapshot.Normalize();
	APRPlayerState* LegacyState = NewObject<APRPlayerState>();
	TestTrue(
		TEXT("Legacy snapshot applies through the runtime bridge"),
		FPRProfileRuntimeBridge::ApplyToPlayerState(LegacyAssaultSnapshot, LegacyState, Diagnostic));
	TestTrue(
		TEXT("Legacy snapshot receives the Assault default Q/E/R loadout"),
		LegacyState->GetRoleComponent()->IsLoadoutValid(
			TEXT("Ability.Role.Assault"),
			LegacyState->GetRoleComponent()->GetCurrentLoadout()));
	TestEqual(
		TEXT("Legacy snapshot provisions all Assault module unlocks"),
		LegacyState->GetRoleComponent()->GetUnlockedModuleIds().Num(),
		3);

	FPRMultiplayerProfileProjection BaselineProjection;
	BaselineProjection.ProfileId = FGuid::NewGuid();
	BaselineProjection.DisplayName = TEXT("Baseline Profile");
	BaselineProjection.ResourceWallet = { FPRProfileResourceBalance{ TEXT("EnergyCrystal"), 11 } };
	BaselineProjection.SelectedRoleId = TEXT("Ability.Role.Assault");
	BaselineProjection.UnlockedRoleIds = { TEXT("Ability.Role.Assault") };
	APRPlayerState* BaselineState = NewObject<APRPlayerState>();
	TestTrue(TEXT("Baseline profile binds before a failed replacement"), BaselineState->BindMultiplayerProfile(BaselineProjection, Diagnostic));
	const TArray<FPRItemInstance> PreviousMissionBackpack = BaselineState->GetMissionStartBackpackItems();
	const TArray<FPRProfileResourceBalance> PreviousMissionResources = BaselineState->GetMissionStartResourceWallet();
	const FName PreviousMissionRole = BaselineState->GetMissionStartRoleId();
	const TArray<FName> PreviousMissionUnlocks = BaselineState->GetMissionStartUnlockedRoleIds();
	const TArray<FName> PreviousMissionModuleUnlocks = BaselineState->GetMissionStartUnlockedRoleModuleIds();
	const FPRRoleLoadout PreviousMissionLoadout = BaselineState->GetMissionStartRoleLoadout();
	FPRMultiplayerProfileProjection UnknownLegacyProjection = BaselineProjection;
	UnknownLegacyProjection.ProfileId = FGuid::NewGuid();
	UnknownLegacyProjection.SelectedRoleId = UnknownRoleId;
	UnknownLegacyProjection.UnlockedRoleIds = { UnknownRoleId };
	UnknownLegacyProjection.UnlockedRoleModuleIds.Reset();
	UnknownLegacyProjection.EquippedRoleModules.Reset();
	TestTrue(
		TEXT("An unknown legacy active role binds without being defaulted or rejected"),
		BaselineState->BindMultiplayerProfile(UnknownLegacyProjection, Diagnostic));
	TestEqual(
		TEXT("Unknown legacy active role remains the compatibility mirror after bind"),
		BaselineState->GetSelectedRoleModule(),
		UnknownRoleId);
	TestTrue(
		TEXT("Unknown legacy active role keeps its empty module payload after bind"),
		BaselineState->GetRoleComponent()->GetCurrentLoadout().Entries.IsEmpty());
	TestTrue(
		TEXT("Unknown legacy active role keeps its empty module unlock payload after bind"),
		BaselineState->GetRoleComponent()->GetUnlockedModuleIds().IsEmpty());
	BaselineState->SetReady(true);
	TestFalse(TEXT("An unknown legacy active role remains blocked from Ready"), BaselineState->IsReady());

	APRPlayerState* ReplacementState = NewObject<APRPlayerState>();
	LobbyState->CopyProperties(ReplacementState);
	TestEqual(TEXT("Seamless replacement retains unknown role loadout entries"), ReplacementState->GetRoleComponent()->GetCurrentLoadout().Entries.Num(), 3);

	FPRPlayerSettlementReceipt Receipt;
	Receipt.ProfileId = Projection.ProfileId;
	Receipt.MissionId = TEXT("Mission.Rift.Test.Hold");
	Receipt.RunId = FGuid::NewGuid();
	Receipt.SettlementId = FGuid::NewGuid();
	Receipt.Result = EPRRiftMissionResult::Success;
	Receipt.bExtracted = true;
	Receipt.SettledRoleId = UnknownRoleId;
	Receipt.SettledUnlockedRoleIds = { UnknownRoleId };
	Receipt.SettledUnlockedRoleModuleIds = Snapshot.UnlockedRoleModuleIds;
	Receipt.SettledEquippedRoleModules = UnknownLayout;
	TestTrue(TEXT("Settlement receipt retains structurally valid unknown role state"), Receipt.IsValid(&Diagnostic));

	return true;
}

#endif
