#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_EngineerRepairDrone.h"
#include "Abilities/GA_EngineerSentry.h"
#include "Abilities/GA_EngineerShieldGenerator.h"
#include "Abilities/GA_MedicFieldHeal.h"
#include "Abilities/GA_MedicPurificationPulse.h"
#include "Abilities/GA_MedicReconScan.h"
#include "Core/PRAssetManager.h"
#include "Deployables/PRDeployableActor.h"
#include "Engine/AssetManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Roles/PREngineerModuleDataAsset.h"
#include "Roles/PRMedicModuleDataAsset.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"

#include <limits>

namespace ProjectRiftRoleDataAssetTestPrivate
{
	constexpr TCHAR AssaultRoleId[] = TEXT("Ability.Role.Assault");
	constexpr TCHAR EngineerRoleId[] = TEXT("Ability.Role.Engineer");
	constexpr TCHAR MedicRoleId[] = TEXT("Ability.Role.Medic");
constexpr TCHAR ChargeModuleId[] = TEXT("Ability.Module.Assault.Charge");
constexpr TCHAR BlastModuleId[] = TEXT("Ability.Module.Assault.Blast");
constexpr TCHAR ShieldModuleId[] = TEXT("Ability.Module.Assault.Shield");
constexpr TCHAR SentryModuleId[] = TEXT("Ability.Module.Engineer.Sentry");
constexpr TCHAR RepairDroneModuleId[] = TEXT("Ability.Module.Engineer.RepairDrone");
	constexpr TCHAR ShieldGeneratorModuleId[] = TEXT("Ability.Module.Engineer.ShieldGenerator");
	constexpr TCHAR FieldHealModuleId[] = TEXT("Ability.Module.Medic.FieldHeal");
	constexpr TCHAR PurificationPulseModuleId[] = TEXT("Ability.Module.Medic.PurificationPulse");
	constexpr TCHAR ReconScanModuleId[] = TEXT("Ability.Module.Medic.ReconScan");

UPRRoleDataAsset* MakeRole()
{
	UPRRoleDataAsset* Role = NewObject<UPRRoleDataAsset>(GetTransientPackage());
	Role->RoleId = AssaultRoleId;
	Role->DisplayName = FText::FromString(TEXT("Assault"));
	Role->Description = FText::FromString(TEXT("Front-line combatant."));
	Role->AllowedModuleIds = { ChargeModuleId, BlastModuleId, ShieldModuleId };
	Role->DefaultLoadout.Entries = {
		{ EPRRoleModuleSlot::Q, ChargeModuleId },
		{ EPRRoleModuleSlot::E, BlastModuleId },
		{ EPRRoleModuleSlot::R, ShieldModuleId }
	};
	Role->EnergyRegenPerSecond = 4.0f;
	return Role;
}

UPRRoleModuleDataAsset* MakeModule(const FName ModuleId, const EPRRoleModuleSlot Slot)
{
	UPRRoleModuleDataAsset* Module = NewObject<UPRRoleModuleDataAsset>(GetTransientPackage());
	Module->ModuleId = ModuleId;
	Module->RoleId = AssaultRoleId;
	Module->Slot = Slot;
	Module->GameplayAbilityClass = UGA_AssaultCharge::StaticClass();
	Module->DisplayName = FText::FromString(ModuleId.ToString());
	Module->Description = FText::FromString(TEXT("Test module."));
	Module->EnergyCost = 15.0f;
	Module->CooldownSeconds = 5.0f;
	return Module;
}

bool HasScanRule(
	const TArray<FString>& Rules,
	const TCHAR* AssetType,
	const TCHAR* AssetBaseClass,
	const TCHAR* Directory)
{
	return Rules.ContainsByPredicate([AssetType, AssetBaseClass, Directory](const FString& Entry)
	{
		return Entry.Contains(FString::Printf(TEXT("PrimaryAssetType=\"%s\""), AssetType))
			&& Entry.Contains(FString::Printf(TEXT("AssetBaseClass=\"%s\""), AssetBaseClass))
			&& Entry.Contains(FString::Printf(TEXT("Path=\"%s\""), Directory))
			&& Entry.Contains(TEXT("CookRule=AlwaysCook"));
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRoleDataAssetsTest,
	"ProjectRift.Roles.DataAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRoleDataAssetsTest::RunTest(const FString& Parameters)
{
	using namespace ProjectRiftRoleDataAssetTestPrivate;

	TestNotNull(TEXT("Role data asset class exists"), UPRRoleDataAsset::StaticClass());
	TestNotNull(TEXT("Role module data asset class exists"), UPRRoleModuleDataAsset::StaticClass());
	TestTrue(TEXT("Role data asset is a primary data asset"), UPRRoleDataAsset::StaticClass()->IsChildOf(UPrimaryDataAsset::StaticClass()));
	TestTrue(TEXT("Role module data asset is a primary data asset"), UPRRoleModuleDataAsset::StaticClass()->IsChildOf(UPrimaryDataAsset::StaticClass()));

	UPRRoleDataAsset* Role = MakeRole();
	UPRRoleModuleDataAsset* Charge = MakeModule(ChargeModuleId, EPRRoleModuleSlot::Q);
	UPRRoleModuleDataAsset* Blast = MakeModule(BlastModuleId, EPRRoleModuleSlot::E);
	UPRRoleModuleDataAsset* Shield = MakeModule(ShieldModuleId, EPRRoleModuleSlot::R);
	const TArray<UPRRoleDataAsset*> Roles{ Role };
	const TArray<UPRRoleModuleDataAsset*> Modules{ Charge, Blast, Shield };

	FString Diagnostic;
	TestTrue(TEXT("Complete role catalog is valid"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	TestEqual(TEXT("Role primary asset ID is canonical"), Role->GetPrimaryAssetId().ToString(), FString(TEXT("ProjectRiftRole:Ability.Role.Assault")));
	TestEqual(TEXT("Role module primary asset ID is canonical"), Charge->GetPrimaryAssetId().ToString(), FString(TEXT("ProjectRiftRoleModule:Ability.Module.Assault.Charge")));
	UPRRoleDataAsset* NoIdRole = MakeRole();
	NoIdRole->RoleId = NAME_None;
	TestFalse(TEXT("Role with no id has no primary asset ID"), NoIdRole->GetPrimaryAssetId().IsValid());
	UPRRoleModuleDataAsset* NoIdModule = MakeModule(TEXT("Temporary.Module"), EPRRoleModuleSlot::Q);
	NoIdModule->ModuleId = NAME_None;
	TestFalse(TEXT("Module with no id has no primary asset ID"), NoIdModule->GetPrimaryAssetId().IsValid());

	Role->DefaultLoadout.Entries[2].Slot = EPRRoleModuleSlot::E;
	TestFalse(TEXT("Default loadout rejects duplicate slots"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->DefaultLoadout.Entries[2].Slot = EPRRoleModuleSlot::R;
	Role->DefaultLoadout.Entries[2].Slot = EPRRoleModuleSlot::None;
	TestFalse(TEXT("Default loadout rejects a missing slot"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->DefaultLoadout.Entries[2].Slot = EPRRoleModuleSlot::R;
	Role->AllowedModuleIds.Remove(ShieldModuleId);
	TestFalse(TEXT("Default loadout must reference an allowed module"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->AllowedModuleIds.Add(ShieldModuleId);
	Role->AllowedModuleIds.Add(ShieldModuleId);
	TestFalse(TEXT("Role rejects duplicate allowed module IDs"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->AllowedModuleIds.Remove(ShieldModuleId);
	Role->AllowedModuleIds.Add(ShieldModuleId);
	Shield->RoleId = TEXT("Ability.Role.Medic");
	TestFalse(TEXT("Module must reference an existing matching role"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->RoleId = AssaultRoleId;
	Shield->Slot = EPRRoleModuleSlot::E;
	TestFalse(TEXT("Default module must match its declared slot"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->Slot = EPRRoleModuleSlot::R;
	Shield->EnergyCost = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Module rejects non-finite energy cost"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->EnergyCost = 15.0f;
	Shield->EnergyCost = -1.0f;
	TestFalse(TEXT("Module rejects negative energy cost"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->EnergyCost = 15.0f;
	Shield->CooldownSeconds = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Module rejects non-finite cooldown"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->CooldownSeconds = -1.0f;
	TestFalse(TEXT("Module rejects negative cooldown"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Shield->CooldownSeconds = 5.0f;
	Role->EnergyRegenPerSecond = 0.0f;
	TestFalse(TEXT("Role rejects non-positive energy regeneration"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->EnergyRegenPerSecond = -1.0f;
	TestFalse(TEXT("Role rejects negative energy regeneration"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->EnergyRegenPerSecond = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Role rejects non-finite energy regeneration"), UPRRoleDataAsset::ValidateCatalog(Roles, Modules, &Diagnostic));
	Role->EnergyRegenPerSecond = 4.0f;
	UPRRoleDataAsset* DuplicateRole = MakeRole();
	const TArray<UPRRoleDataAsset*> DuplicateRoles{ Role, DuplicateRole };
	TestFalse(TEXT("Catalog rejects duplicate role IDs"), UPRRoleDataAsset::ValidateCatalog(DuplicateRoles, Modules, &Diagnostic));
	UPRRoleModuleDataAsset* DuplicateModule = MakeModule(ChargeModuleId, EPRRoleModuleSlot::Q);
	const TArray<UPRRoleModuleDataAsset*> DuplicateModules{ Charge, Blast, Shield, DuplicateModule };
	TestFalse(TEXT("Catalog rejects duplicate module IDs"), UPRRoleDataAsset::ValidateCatalog(Roles, DuplicateModules, &Diagnostic));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRRoleAssetManagerTest,
	"ProjectRift.Assets.Manager.Roles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRRoleAssetManagerTest::RunTest(const FString& Parameters)
{
	using namespace ProjectRiftRoleDataAssetTestPrivate;

	TArray<FString> PrimaryAssetTypesToScan;
	GConfig->GetArray(
		TEXT("/Script/Engine.AssetManagerSettings"),
		TEXT("PrimaryAssetTypesToScan"),
		PrimaryAssetTypesToScan,
		GGameIni);
	TestTrue(TEXT("AssetManager scans ProjectRiftRole as AlwaysCook"), HasScanRule(PrimaryAssetTypesToScan, TEXT("ProjectRiftRole"), TEXT("/Script/ProjectA.PRRoleDataAsset"), TEXT("/Game/ProjectRift/Roles")));
	TestTrue(TEXT("AssetManager scans ProjectRiftRoleModule as AlwaysCook"), HasScanRule(PrimaryAssetTypesToScan, TEXT("ProjectRiftRoleModule"), TEXT("/Script/ProjectA.PRRoleModuleDataAsset"), TEXT("/Game/ProjectRift/RoleModules")));
	TestEqual(TEXT("Canonical role primary ID"), UPRAssetManager::MakeRolePrimaryAssetId(AssaultRoleId).ToString(), FString(TEXT("ProjectRiftRole:Ability.Role.Assault")));
	TestEqual(TEXT("Canonical role module primary ID"), UPRAssetManager::MakeRoleModulePrimaryAssetId(ChargeModuleId).ToString(), FString(TEXT("ProjectRiftRoleModule:Ability.Module.Assault.Charge")));
	TestFalse(TEXT("Empty role primary ID is invalid"), UPRAssetManager::MakeRolePrimaryAssetId(NAME_None).IsValid());
	TestFalse(TEXT("Empty role module primary ID is invalid"), UPRAssetManager::MakeRoleModulePrimaryAssetId(NAME_None).IsValid());

	UPRAssetManager* Manager = UPRAssetManager::Get();
	TestNotNull(TEXT("Typed ProjectRift AssetManager exists"), Manager);
	if (Manager)
	{
		TArray<UPRRoleDataAsset*> Roles;
		TArray<UPRRoleModuleDataAsset*> Modules;
		TestTrue(TEXT("Shipped role catalog loads and validates"), Manager->LoadRoleCatalog(Roles, Modules));
		TestEqual(TEXT("All three shipped starter roles are registered"), Roles.Num(), 3);
		TestEqual(TEXT("All shipped role modules are registered"), Modules.Num(), 9);
		TestTrue(TEXT("Assault role content asset is registered and loadable"), Roles.ContainsByPredicate([](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId == AssaultRoleId && Role->GetPrimaryAssetId() == UPRAssetManager::MakeRolePrimaryAssetId(AssaultRoleId);
		}));
		const UPRRoleDataAsset* const* EngineerRoleEntry = Roles.FindByPredicate([](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId == EngineerRoleId;
		});
		const UPRRoleDataAsset* EngineerRole = EngineerRoleEntry ? *EngineerRoleEntry : nullptr;
		TestNotNull(TEXT("Engineer role content asset is registered and loadable"), EngineerRole);
		if (EngineerRole)
		{
			TestTrue(TEXT("Engineer is starter-unlocked"), EngineerRole->bStarterUnlocked);
			TestEqual(TEXT("Engineer energy regeneration is configured"), EngineerRole->EnergyRegenPerSecond, 5.0f);
			TestEqual(TEXT("Engineer has exactly three allowed modules"), EngineerRole->AllowedModuleIds.Num(), 3);
		}
		const UPRRoleDataAsset* const* MedicRoleEntry = Roles.FindByPredicate([](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId == MedicRoleId;
		});
		const UPRRoleDataAsset* MedicRole = MedicRoleEntry ? *MedicRoleEntry : nullptr;
		TestNotNull(TEXT("Medic role content asset is registered and loadable"), MedicRole);
		if (MedicRole)
		{
			TestTrue(TEXT("Medic is starter-unlocked"), MedicRole->bStarterUnlocked);
			TestEqual(TEXT("Medic energy regeneration is configured"), MedicRole->EnergyRegenPerSecond, 6.0f);
			TestEqual(TEXT("Medic has exactly three allowed modules"), MedicRole->AllowedModuleIds.Num(), 3);
		}
		const TSet<FName> ExpectedModuleIds{
			ChargeModuleId, BlastModuleId, ShieldModuleId,
			SentryModuleId, RepairDroneModuleId, ShieldGeneratorModuleId,
			FieldHealModuleId, PurificationPulseModuleId, ReconScanModuleId };
		TestTrue(TEXT("All shipped role module content assets are loadable and valid"), Modules.FilterByPredicate([&ExpectedModuleIds](const UPRRoleModuleDataAsset* Module)
		{
			return Module && ExpectedModuleIds.Contains(Module->ModuleId)
				&& Module->GetPrimaryAssetId() == UPRAssetManager::MakeRoleModulePrimaryAssetId(Module->ModuleId)
				&& Module->ValidateDefinition();
		}).Num() == ExpectedModuleIds.Num());
		const UPRRoleModuleDataAsset* const* SentryEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == SentryModuleId;
		});
		const UPREngineerModuleDataAsset* Sentry = SentryEntry ? Cast<UPREngineerModuleDataAsset>(*SentryEntry) : nullptr;
		TestNotNull(TEXT("Engineer sentry uses the deployable module schema"), Sentry);
		if (Sentry)
		{
			AddInfo(FString::Printf(TEXT("Resolved sentry deployable class: %s"), *GetNameSafe(Sentry->DeployableActorClass.Get())));
			TestEqual(TEXT("Sentry uses the Q slot"), Sentry->Slot, EPRRoleModuleSlot::Q);
			TestEqual(TEXT("Sentry has the configured energy cost"), Sentry->EnergyCost, 20.0f);
			TestEqual(TEXT("Sentry has the configured cooldown"), Sentry->CooldownSeconds, 10.0f);
			TestEqual(TEXT("Sentry has the configured lifetime"), Sentry->LifetimeSeconds, 12.0f);
			TestEqual(TEXT("Sentry has the configured damage"), Sentry->PrimaryMagnitude, 5.0f);
			TestEqual(TEXT("Sentry has the configured fire interval"), Sentry->EffectIntervalSeconds, 0.6f);
			TestTrue(TEXT("Sentry uses the Engineer sentry ability"), Sentry->GameplayAbilityClass == UGA_EngineerSentry::StaticClass());
			TestEqual(TEXT("Sentry uses its dedicated runtime actor"), GetNameSafe(Sentry->DeployableActorClass.Get()), FString(TEXT("PRSentryDeployable")));
		}
		const UPRRoleModuleDataAsset* const* RepairDroneEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == RepairDroneModuleId;
		});
		const UPREngineerModuleDataAsset* RepairDrone = RepairDroneEntry ? Cast<UPREngineerModuleDataAsset>(*RepairDroneEntry) : nullptr;
		TestNotNull(TEXT("Engineer repair drone uses the deployable module schema"), RepairDrone);
		if (RepairDrone)
		{
			AddInfo(FString::Printf(TEXT("Resolved repair drone deployable class: %s"), *GetNameSafe(RepairDrone->DeployableActorClass.Get())));
			TestEqual(TEXT("Repair drone uses the E slot"), RepairDrone->Slot, EPRRoleModuleSlot::E);
			TestEqual(TEXT("Repair drone has the configured energy cost"), RepairDrone->EnergyCost, 25.0f);
			TestEqual(TEXT("Repair drone has the configured cooldown"), RepairDrone->CooldownSeconds, 12.0f);
			TestEqual(TEXT("Repair drone heals once per second"), RepairDrone->EffectIntervalSeconds, 1.0f);
			TestEqual(TEXT("Repair drone has the configured repair amount"), RepairDrone->PrimaryMagnitude, 6.0f);
			TestTrue(TEXT("Repair drone uses the Engineer repair ability"), RepairDrone->GameplayAbilityClass == UGA_EngineerRepairDrone::StaticClass());
			TestEqual(TEXT("Repair drone uses its dedicated runtime actor"), GetNameSafe(RepairDrone->DeployableActorClass.Get()), FString(TEXT("PRRepairDroneDeployable")));
		}
		const UPRRoleModuleDataAsset* const* ShieldGeneratorEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == ShieldGeneratorModuleId;
		});
		const UPREngineerModuleDataAsset* ShieldGenerator = ShieldGeneratorEntry ? Cast<UPREngineerModuleDataAsset>(*ShieldGeneratorEntry) : nullptr;
		TestNotNull(TEXT("Engineer shield generator uses the deployable module schema"), ShieldGenerator);
		if (ShieldGenerator)
		{
			AddInfo(FString::Printf(TEXT("Resolved shield generator deployable class: %s"), *GetNameSafe(ShieldGenerator->DeployableActorClass.Get())));
			TestEqual(TEXT("Shield generator uses the R slot"), ShieldGenerator->Slot, EPRRoleModuleSlot::R);
			TestEqual(TEXT("Shield generator has the configured energy cost"), ShieldGenerator->EnergyCost, 40.0f);
			TestEqual(TEXT("Shield generator has the configured cooldown"), ShieldGenerator->CooldownSeconds, 20.0f);
			TestEqual(TEXT("Shield generator has the configured lifetime"), ShieldGenerator->LifetimeSeconds, 10.0f);
			TestEqual(TEXT("Shield generator has the configured shield amount"), ShieldGenerator->PrimaryMagnitude, 30.0f);
			TestTrue(TEXT("Shield generator uses the Engineer shield ability"), ShieldGenerator->GameplayAbilityClass == UGA_EngineerShieldGenerator::StaticClass());
			TestEqual(TEXT("Shield generator uses its dedicated runtime actor"), GetNameSafe(ShieldGenerator->DeployableActorClass.Get()), FString(TEXT("PRShieldGeneratorDeployable")));
		}
		const UPRRoleModuleDataAsset* const* FieldHealEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == FieldHealModuleId;
		});
		const UPRMedicModuleDataAsset* FieldHeal = FieldHealEntry ? Cast<UPRMedicModuleDataAsset>(*FieldHealEntry) : nullptr;
		TestNotNull(TEXT("Medic field heal uses the Medic module schema"), FieldHeal);
		if (FieldHeal)
		{
			TestEqual(TEXT("Field heal uses the Q slot"), FieldHeal->Slot, EPRRoleModuleSlot::Q);
			TestEqual(TEXT("Field heal costs twenty energy"), FieldHeal->EnergyCost, 20.0f);
			TestEqual(TEXT("Field heal has a six second cooldown"), FieldHeal->CooldownSeconds, 6.0f);
			TestEqual(TEXT("Field heal restores thirty base health"), FieldHeal->PrimaryMagnitude, 30.0f);
			TestEqual(TEXT("Field heal reaches 1800 units"), FieldHeal->TargetRange, 1800.0f);
			TestEqual(TEXT("Field heal has the correct kind"), FieldHeal->MedicKind, EPRMedicModuleKind::FieldHeal);
			TestTrue(TEXT("Field heal uses the native Medic ability"), FieldHeal->GameplayAbilityClass == UGA_MedicFieldHeal::StaticClass());
		}
		const UPRRoleModuleDataAsset* const* PurificationEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == PurificationPulseModuleId;
		});
		const UPRMedicModuleDataAsset* Purification = PurificationEntry ? Cast<UPRMedicModuleDataAsset>(*PurificationEntry) : nullptr;
		TestNotNull(TEXT("Medic purification uses the Medic module schema"), Purification);
		if (Purification)
		{
			TestEqual(TEXT("Purification uses the E slot"), Purification->Slot, EPRRoleModuleSlot::E);
			TestEqual(TEXT("Purification costs twenty-five energy"), Purification->EnergyCost, 25.0f);
			TestEqual(TEXT("Purification has a twelve second cooldown"), Purification->CooldownSeconds, 12.0f);
			TestEqual(TEXT("Purification has a 1000 unit radius"), Purification->EffectRadius, 1000.0f);
			TestEqual(TEXT("Purification has the correct kind"), Purification->MedicKind, EPRMedicModuleKind::PurificationPulse);
			TestTrue(TEXT("Purification uses the native Medic ability"), Purification->GameplayAbilityClass == UGA_MedicPurificationPulse::StaticClass());
		}
		const UPRRoleModuleDataAsset* const* ReconEntry = Modules.FindByPredicate([](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == ReconScanModuleId;
		});
		const UPRMedicModuleDataAsset* Recon = ReconEntry ? Cast<UPRMedicModuleDataAsset>(*ReconEntry) : nullptr;
		TestNotNull(TEXT("Medic recon uses the Medic module schema"), Recon);
		if (Recon)
		{
			TestEqual(TEXT("Recon uses the R slot"), Recon->Slot, EPRRoleModuleSlot::R);
			TestEqual(TEXT("Recon costs forty energy"), Recon->EnergyCost, 40.0f);
			TestEqual(TEXT("Recon has a twenty second cooldown"), Recon->CooldownSeconds, 20.0f);
			TestEqual(TEXT("Recon has a 3500 unit radius"), Recon->EffectRadius, 3500.0f);
			TestEqual(TEXT("Recon lasts eight seconds"), Recon->DurationSeconds, 8.0f);
			TestEqual(TEXT("Recon has the correct kind"), Recon->MedicKind, EPRMedicModuleKind::ReconScan);
			TestTrue(TEXT("Recon uses the native Medic ability"), Recon->GameplayAbilityClass == UGA_MedicReconScan::StaticClass());
		}
	}
	return true;
}

#endif
