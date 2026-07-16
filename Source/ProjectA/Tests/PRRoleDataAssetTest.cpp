#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultCharge.h"
#include "Core/PRAssetManager.h"
#include "Engine/AssetManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"

#include <limits>

namespace ProjectRiftRoleDataAssetTestPrivate
{
constexpr TCHAR AssaultRoleId[] = TEXT("Ability.Role.Assault");
constexpr TCHAR ChargeModuleId[] = TEXT("Ability.Module.Assault.Charge");
constexpr TCHAR BlastModuleId[] = TEXT("Ability.Module.Assault.Blast");
constexpr TCHAR ShieldModuleId[] = TEXT("Ability.Module.Assault.Shield");

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
		TestTrue(TEXT("Assault role catalog loads and validates"), Manager->LoadRoleCatalog(Roles, Modules));
		TestEqual(TEXT("Only the shipped Assault role is registered"), Roles.Num(), 1);
		TestEqual(TEXT("Only the shipped Assault modules are registered"), Modules.Num(), 3);
		TestTrue(TEXT("Assault role content asset is registered and loadable"), Roles.ContainsByPredicate([](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId == AssaultRoleId && Role->GetPrimaryAssetId() == UPRAssetManager::MakeRolePrimaryAssetId(AssaultRoleId);
		}));
		TestTrue(TEXT("No unrelated role content assets are registered"), Roles.FilterByPredicate([](const UPRRoleDataAsset* Role)
		{
			return Role && Role->RoleId != AssaultRoleId;
		}).IsEmpty());
		const TSet<FName> ExpectedModuleIds{ ChargeModuleId, BlastModuleId, ShieldModuleId };
		TestTrue(TEXT("All shipped Assault module content assets are loadable and valid"), Modules.FilterByPredicate([&ExpectedModuleIds](const UPRRoleModuleDataAsset* Module)
		{
			return Module && ExpectedModuleIds.Contains(Module->ModuleId)
				&& Module->RoleId == AssaultRoleId
				&& Module->GetPrimaryAssetId() == UPRAssetManager::MakeRoleModulePrimaryAssetId(Module->ModuleId)
				&& Module->ValidateDefinition();
		}).Num() == ExpectedModuleIds.Num());
		TestTrue(TEXT("No unrelated role module content assets are registered"), Modules.FilterByPredicate([&ExpectedModuleIds](const UPRRoleModuleDataAsset* Module)
		{
			return !Module || !ExpectedModuleIds.Contains(Module->ModuleId);
		}).IsEmpty());
	}
	return true;
}

#endif
