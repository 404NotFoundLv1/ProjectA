#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Core/PRAssetManager.h"
#include "Characters/PRCharacter.h"
#include "Multiplayer/PRMultiplayerProfileTypes.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleComponent.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRoleLoadoutTest,
	"ProjectRift.Roles.Loadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRoleLoadoutTest::RunTest(const FString& Parameters)
{
	const FName AssaultRoleId(TEXT("Ability.Role.Assault"));
	const FName ChargeModuleId(TEXT("Ability.Module.Assault.Charge"));
	const FName BlastModuleId(TEXT("Ability.Module.Assault.Blast"));
	const FName ShieldModuleId(TEXT("Ability.Module.Assault.Shield"));

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TestNotNull(TEXT("ProjectRift AssetManager is available"), AssetManager);
	if (AssetManager)
	{
		UPRRoleDataAsset* AssaultRole = AssetManager->LoadRoleSync(AssaultRoleId);
		UPRRoleModuleDataAsset* ChargeModule = AssetManager->LoadRoleModuleSync(ChargeModuleId);
		UPRRoleModuleDataAsset* BlastModule = AssetManager->LoadRoleModuleSync(BlastModuleId);
		UPRRoleModuleDataAsset* ShieldModule = AssetManager->LoadRoleModuleSync(ShieldModuleId);
		TestNotNull(TEXT("The real Assault role asset loads"), AssaultRole);
		TestNotNull(TEXT("The real Assault Charge module asset loads"), ChargeModule);
		TestNotNull(TEXT("The real Assault Blast module asset loads"), BlastModule);
		TestNotNull(TEXT("The real Assault Shield module asset loads"), ShieldModule);
	}

	// A pawn may be initialized after its PlayerState has restored a valid profile loadout.
	// Keep a deliberately reordered, still-valid default definition separate from the
	// restored loadout so this catches an unconditional default-provisioning call.
	UPRRoleDataAsset* AssaultRoleForPawnInitialization = AssetManager ? AssetManager->LoadRoleSync(AssaultRoleId) : nullptr;
	if (AssaultRoleForPawnInitialization)
	{
		const FPRRoleLoadout RestoredLoadout = AssaultRoleForPawnInitialization->DefaultLoadout;
		FPRRoleLoadout ReorderedDefaultLoadout = RestoredLoadout;
		if (ReorderedDefaultLoadout.Entries.Num() >= 2)
		{
			Swap(ReorderedDefaultLoadout.Entries[0], ReorderedDefaultLoadout.Entries[1]);
		}

		TestTrue(TEXT("The reordered role default remains structurally valid"), ReorderedDefaultLoadout.IsStructurallyValid());
		AssaultRoleForPawnInitialization->DefaultLoadout = ReorderedDefaultLoadout;

		FTestWorldWrapper PawnInitializationWorldWrapper;
		TestTrue(TEXT("A pawn-initialization test world is created"), PawnInitializationWorldWrapper.CreateTestWorld(EWorldType::Game));
		TestTrue(TEXT("The pawn-initialization test world begins play"), PawnInitializationWorldWrapper.BeginPlayInTestWorld());
		UWorld* PawnInitializationWorld = PawnInitializationWorldWrapper.GetTestWorld();
		APRPlayerState* RestoredPlayerState = PawnInitializationWorld ? PawnInitializationWorld->SpawnActor<APRPlayerState>() : nullptr;
		UPRRoleComponent* RestoredRoleComponent = RestoredPlayerState ? RestoredPlayerState->GetRoleComponent() : nullptr;
		APlayerController* RestoredController = PawnInitializationWorld ? PawnInitializationWorld->SpawnActor<APlayerController>() : nullptr;
		APRCharacter* RestoredPawn = PawnInitializationWorld ? PawnInitializationWorld->SpawnActor<APRCharacter>() : nullptr;
		TestNotNull(TEXT("Restored PlayerState exists for pawn initialization"), RestoredPlayerState);
		TestNotNull(TEXT("Restored role component exists for pawn initialization"), RestoredRoleComponent);
		TestNotNull(TEXT("Restored controller exists for pawn initialization"), RestoredController);
		TestNotNull(TEXT("Restored pawn exists for pawn initialization"), RestoredPawn);
		if (RestoredPlayerState && RestoredRoleComponent && RestoredController && RestoredPawn)
		{
			RestoredRoleComponent->ApplyProfileRoleState(
				AssaultRoleId,
				{ AssaultRoleId },
				RestoredLoadout,
				{ ChargeModuleId, BlastModuleId, ShieldModuleId });
			TestTrue(
				TEXT("The restored loadout is valid before pawn initialization"),
				RestoredRoleComponent->IsLoadoutValid(AssaultRoleId, RestoredRoleComponent->GetCurrentLoadout()));
			RestoredController->SetPlayerState(RestoredPlayerState);
			RestoredController->Possess(RestoredPawn);
			TestEqual(
				TEXT("Pawn initialization preserves the already-valid restored loadout instead of provisioning the role default"),
				RestoredRoleComponent->GetCurrentLoadout().Entries,
				RestoredLoadout.Entries);
		}

		AssaultRoleForPawnInitialization->DefaultLoadout = RestoredLoadout;
	}

	UClass* RoleComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRoleComponent"));
	TestNotNull(TEXT("PlayerState-owned data-driven role component exists"), RoleComponentClass);
	if (RoleComponentClass)
	{
		TestNotNull(
			TEXT("Role component exposes a replicated current loadout"),
			FindFProperty<FStructProperty>(RoleComponentClass, TEXT("CurrentLoadout")));
		TestNotNull(
			TEXT("Role component exposes atomic loadout application"),
			RoleComponentClass->FindFunctionByName(TEXT("ApplyLoadout")));
	}

	const FObjectProperty* RoleComponentProperty = FindFProperty<FObjectProperty>(APRPlayerState::StaticClass(), TEXT("RoleComponent"));
	TestNotNull(TEXT("APRPlayerState owns the role component"), RoleComponentProperty);
	TestNotNull(TEXT("APRPlayerState exposes GetRoleComponent"), APRPlayerState::StaticClass()->FindFunctionByName(TEXT("GetRoleComponent")));

	APRPlayerState* MutablePlayerState = NewObject<APRPlayerState>();
	UPRRoleComponent* RoleComponent = MutablePlayerState ? MutablePlayerState->GetRoleComponent() : nullptr;
	TestNotNull(TEXT("A PlayerState creates its role component"), RoleComponent);
	if (!MutablePlayerState || !RoleComponent)
	{
		return false;
	}

	MutablePlayerState->SetSelectedRoleModule(TEXT("Legacy.Unknown.Role"));
	TestFalse(TEXT("Unknown legacy roles do not silently default"), RoleComponent->EnsureDefaultLoadoutForSelectedRole());
	TestEqual(
		TEXT("Unknown legacy role mirror survives component defaulting"),
		MutablePlayerState->GetSelectedRoleModule(),
		FName(TEXT("Legacy.Unknown.Role")));

	MutablePlayerState->SetSelectedRoleModule(AssaultRoleId);
	const bool bDefaultedAssault = RoleComponent->EnsureDefaultLoadoutForSelectedRole();
	TestTrue(TEXT("Assault selection provisions its data-asset default loadout"), bDefaultedAssault);
	if (!bDefaultedAssault)
	{
		return false;
	}
	const FPRRoleLoadout& AssaultLoadout = RoleComponent->GetCurrentLoadout();
	TestTrue(TEXT("Default Assault loadout is structurally complete"), AssaultLoadout.IsStructurallyValid());
	const FPRRoleModuleSlotEntry* ChargeEntry = AssaultLoadout.Entries.FindByPredicate(
		[](const FPRRoleModuleSlotEntry& Entry) { return Entry.Slot == EPRRoleModuleSlot::Q; });
	const FPRRoleModuleSlotEntry* BlastEntry = AssaultLoadout.Entries.FindByPredicate(
		[](const FPRRoleModuleSlotEntry& Entry) { return Entry.Slot == EPRRoleModuleSlot::E; });
	const FPRRoleModuleSlotEntry* ShieldEntry = AssaultLoadout.Entries.FindByPredicate(
		[](const FPRRoleModuleSlotEntry& Entry) { return Entry.Slot == EPRRoleModuleSlot::R; });
	TestEqual(TEXT("Default Q is Assault Charge"), ChargeEntry ? ChargeEntry->ModuleId : NAME_None, ChargeModuleId);
	TestEqual(TEXT("Default E is Assault Blast"), BlastEntry ? BlastEntry->ModuleId : NAME_None, BlastModuleId);
	TestEqual(TEXT("Default R is Assault Shield"), ShieldEntry ? ShieldEntry->ModuleId : NAME_None, ShieldModuleId);

	RoleComponent->ApplyProfileRoleState(AssaultRoleId, {}, AssaultLoadout, {});
	TestFalse(
		TEXT("A catalog-valid loadout without permanent role/module unlocks is not valid for play"),
		RoleComponent->IsLoadoutValid(AssaultRoleId, RoleComponent->GetCurrentLoadout()));
	RoleComponent->ApplyProfileRoleState(
		AssaultRoleId,
		{ AssaultRoleId },
		AssaultLoadout,
		{ ChargeModuleId, BlastModuleId, ShieldModuleId });
	TestTrue(
		TEXT("The same catalog-valid loadout becomes valid after its permanent unlocks are restored"),
		RoleComponent->IsLoadoutValid(AssaultRoleId, RoleComponent->GetCurrentLoadout()));

	// Failure after the candidate modules are granted must not destroy the live
	// module specs.  Force the non-matching path and make regeneration invalid;
	// a failed refresh must leave the previously granted specs intact.
	UPRRoleDataAsset* AssaultRoleForAtomicRefresh = AssetManager ? AssetManager->LoadRoleSync(AssaultRoleId) : nullptr;
	UPRAbilitySystemComponent* RoleAbilitySystem = MutablePlayerState->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("The role PlayerState owns an ASC for atomic grant validation"), RoleAbilitySystem);
	if (AssaultRoleForAtomicRefresh && RoleAbilitySystem)
	{
		const float OriginalEnergyRegen = AssaultRoleForAtomicRefresh->EnergyRegenPerSecond;
		FGameplayAbilitySpecHandle OriginalHandle;
		UObject* OriginalSourceObject = nullptr;
		for (const FGameplayAbilitySpec& Spec : RoleAbilitySystem->GetActivatableAbilities())
		{
			if (Spec.SourceObject.Get() && Spec.SourceObject.Get()->IsA<UPRRoleModuleDataAsset>())
			{
				OriginalHandle = Spec.Handle;
				OriginalSourceObject = Spec.SourceObject.Get();
				break;
			}
		}
		FGameplayAbilitySpec* MutableSpec = OriginalHandle.IsValid() ? RoleAbilitySystem->FindAbilitySpecFromHandle(OriginalHandle) : nullptr;
		TestNotNull(TEXT("A live role ability spec is available for atomic grant validation"), MutableSpec);
		if (MutableSpec)
		{
			MutableSpec->SourceObject = NewObject<UPRRoleModuleDataAsset>(RoleComponent);
			AssaultRoleForAtomicRefresh->EnergyRegenPerSecond = 0.0f;
			TestFalse(TEXT("An invalid regeneration definition makes the refresh fail"), RoleComponent->RefreshGrantedAbilities());
			TestNotNull(
				TEXT("A failed refresh preserves the original live module spec"),
				RoleAbilitySystem->FindAbilitySpecFromHandle(OriginalHandle));
			MutableSpec = RoleAbilitySystem->FindAbilitySpecFromHandle(OriginalHandle);
			if (MutableSpec)
			{
				MutableSpec->SourceObject = OriginalSourceObject;
			}
			AssaultRoleForAtomicRefresh->EnergyRegenPerSecond = OriginalEnergyRegen;
		}
	}

	FPRRoleLoadout WrongSlotLoadout = AssaultLoadout;
	if (FPRRoleModuleSlotEntry* QEntry = WrongSlotLoadout.Entries.FindByPredicate(
		[](const FPRRoleModuleSlotEntry& Entry) { return Entry.Slot == EPRRoleModuleSlot::Q; }))
	{
		QEntry->ModuleId = ShieldModuleId;
	}
	TestFalse(
		TEXT("Wrong-slot module is rejected before a loadout transaction begins"),
		RoleComponent->IsLoadoutValid(AssaultRoleId, WrongSlotLoadout));
	const FPRRoleModuleSlotEntry* CurrentQEntry = RoleComponent->GetCurrentLoadout().Entries.FindByPredicate(
		[](const FPRRoleModuleSlotEntry& Entry) { return Entry.Slot == EPRRoleModuleSlot::Q; });
	TestEqual(TEXT("Rejected loadout leaves the current Q module unchanged"), CurrentQEntry ? CurrentQEntry->ModuleId : NAME_None, ChargeModuleId);

	TestEqual(
		TEXT("Manual loadout replacement fails closed without an authoritative lobby GameMode"),
		RoleComponent->ApplyLoadout(AssaultRoleId, AssaultLoadout),
		EPRRoleLoadoutApplyResult::NotInLobby);

	FTestWorldWrapper NonLobbyWorldWrapper;
	TestTrue(TEXT("A non-lobby world is created for loadout authorization"), NonLobbyWorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* NonLobbyWorld = NonLobbyWorldWrapper.GetTestWorld();
	if (NonLobbyWorld && NonLobbyWorld->GetWorldSettings())
	{
		NonLobbyWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	}
	TestTrue(TEXT("The non-lobby world begins play"), NonLobbyWorldWrapper.BeginPlayInTestWorld());
	if (NonLobbyWorldWrapper.HasFailed())
	{
		NonLobbyWorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	APRPlayerState* NonLobbyPlayerState = NonLobbyWorld ? NonLobbyWorld->SpawnActor<APRPlayerState>() : nullptr;
	UPRRoleComponent* NonLobbyRoleComponent = NonLobbyPlayerState ? NonLobbyPlayerState->GetRoleComponent() : nullptr;
	TestNotNull(TEXT("A non-lobby PlayerState owns its role component"), NonLobbyRoleComponent);
	if (NonLobbyPlayerState && NonLobbyRoleComponent)
	{
		NonLobbyPlayerState->SetSelectedRoleModule(AssaultRoleId);
		TestTrue(TEXT("Automatic default provisioning remains allowed outside a lobby transaction"), NonLobbyRoleComponent->EnsureDefaultLoadoutForSelectedRole());
		TestEqual(
			TEXT("Manual loadout replacement rejects an actual non-lobby GameMode"),
			NonLobbyRoleComponent->ApplyLoadout(AssaultRoleId, NonLobbyRoleComponent->GetCurrentLoadout()),
			EPRRoleLoadoutApplyResult::NotInLobby);
	}

	APRPlayerState* BoundPlayerState = NewObject<APRPlayerState>();
	FPRMultiplayerProfileProjection BoundProjection;
	BoundProjection.ProfileId = FGuid::NewGuid();
	BoundProjection.DisplayName = TEXT("Loadout Ready Test");
	FString BindingDiagnostic;
	TestTrue(TEXT("A multiplayer profile binds for ready validation"), BoundPlayerState->BindMultiplayerProfile(BoundProjection, BindingDiagnostic));
	UPRRoleComponent* BoundRoleComponent = BoundPlayerState->GetRoleComponent();
	TestNotNull(TEXT("A bound PlayerState retains its role component"), BoundRoleComponent);
	if (BoundRoleComponent)
	{
		BoundRoleComponent->ApplyProfileRoleState(
			TEXT("Legacy.Unknown.Role"),
			{ TEXT("Legacy.Unknown.Role") },
			FPRRoleLoadout(),
			{});
		BoundPlayerState->SetReady(true);
		TestFalse(TEXT("A bound profile with an invalid loadout cannot become ready"), BoundPlayerState->IsReady());
		TestEqual(
			TEXT("Ready rejection preserves the unknown selected role mirror"),
			BoundPlayerState->GetSelectedRoleModule(),
			FName(TEXT("Legacy.Unknown.Role")));
	}
	return true;
}

#endif
