#include "Roles/PRRoleDataAsset.h"

#include "Roles/PRRoleModuleDataAsset.h"

const FPrimaryAssetType UPRRoleDataAsset::RolePrimaryAssetType(TEXT("ProjectRiftRole"));

FPrimaryAssetId UPRRoleDataAsset::GetPrimaryAssetId() const
{
	return RoleId.IsNone() ? FPrimaryAssetId() : FPrimaryAssetId(RolePrimaryAssetType, RoleId);
}

bool UPRRoleDataAsset::ValidateDefinition(FString* OutDiagnostic) const
{
	if (RoleId.IsNone() || DisplayName.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role id and display name are required."); }
		return false;
	}
	if (!FMath::IsFinite(EnergyRegenPerSecond) || EnergyRegenPerSecond <= 0.0f)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role energy regeneration must be finite and positive."); }
		return false;
	}
	if (AllowedModuleIds.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role must allow at least one module."); }
		return false;
	}
	TSet<FName> SeenModuleIds;
	for (const FName ModuleId : AllowedModuleIds)
	{
		if (ModuleId.IsNone() || SeenModuleIds.Contains(ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Role allowed modules contain an invalid or duplicate id."); }
			return false;
		}
		SeenModuleIds.Add(ModuleId);
	}
	if (!DefaultLoadout.IsStructurallyValid(OutDiagnostic))
	{
		return false;
	}
	for (const FPRRoleModuleSlotEntry& Entry : DefaultLoadout.Entries)
	{
		if (!SeenModuleIds.Contains(Entry.ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Role default loadout contains a module not allowed by the role."); }
			return false;
		}
	}
	return true;
}

bool UPRRoleDataAsset::ValidateCatalog(
	const TArray<UPRRoleDataAsset*>& Roles,
	const TArray<UPRRoleModuleDataAsset*>& Modules,
	FString* OutDiagnostic)
{
	TMap<FName, const UPRRoleDataAsset*> RoleById;
	for (const UPRRoleDataAsset* Role : Roles)
	{
		FString Diagnostic;
		if (!Role || !Role->ValidateDefinition(&Diagnostic) || RoleById.Contains(Role->RoleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = Diagnostic.IsEmpty() ? TEXT("Role catalog contains a null or duplicate role.") : Diagnostic; }
			return false;
		}
		RoleById.Add(Role->RoleId, Role);
	}

	TMap<FName, const UPRRoleModuleDataAsset*> ModuleById;
	for (const UPRRoleModuleDataAsset* Module : Modules)
	{
		FString Diagnostic;
		if (!Module || !Module->ValidateDefinition(&Diagnostic) || ModuleById.Contains(Module->ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = Diagnostic.IsEmpty() ? TEXT("Role module catalog contains a null or duplicate module.") : Diagnostic; }
			return false;
		}
		if (!RoleById.Contains(Module->RoleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("Role module %s references an unknown role."), *Module->ModuleId.ToString()); }
			return false;
		}
		ModuleById.Add(Module->ModuleId, Module);
	}

	for (const TPair<FName, const UPRRoleDataAsset*>& Pair : RoleById)
	{
		const UPRRoleDataAsset* Role = Pair.Value;
		for (const FName ModuleId : Role->AllowedModuleIds)
		{
			const UPRRoleModuleDataAsset* const* Module = ModuleById.Find(ModuleId);
			if (!Module || (*Module)->RoleId != Role->RoleId)
			{
				if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("Role %s allows an unknown or foreign module %s."), *Role->RoleId.ToString(), *ModuleId.ToString()); }
				return false;
			}
		}
		for (const FPRRoleModuleSlotEntry& Entry : Role->DefaultLoadout.Entries)
		{
			const UPRRoleModuleDataAsset* const* Module = ModuleById.Find(Entry.ModuleId);
			if (!Module || (*Module)->RoleId != Role->RoleId || (*Module)->Slot != Entry.Slot)
			{
				if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("Role %s default module %s does not match its slot or role."), *Role->RoleId.ToString(), *Entry.ModuleId.ToString()); }
				return false;
			}
		}
	}
	return true;
}
