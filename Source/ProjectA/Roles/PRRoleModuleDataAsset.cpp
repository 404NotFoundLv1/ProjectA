#include "Roles/PRRoleModuleDataAsset.h"

const FPrimaryAssetType UPRRoleModuleDataAsset::RoleModulePrimaryAssetType(TEXT("ProjectRiftRoleModule"));

FPrimaryAssetId UPRRoleModuleDataAsset::GetPrimaryAssetId() const
{
	return ModuleId.IsNone() ? FPrimaryAssetId() : FPrimaryAssetId(RoleModulePrimaryAssetType, ModuleId);
}

bool UPRRoleModuleDataAsset::ValidateDefinition(FString* OutDiagnostic) const
{
	if (ModuleId.IsNone() || RoleId.IsNone() || Slot == EPRRoleModuleSlot::None
		|| !GameplayAbilityClass || DisplayName.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role module id, role id, slot, ability class, and display name are required."); }
		return false;
	}
	if (!FMath::IsFinite(EnergyCost) || EnergyCost < 0.0f
		|| !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0.0f)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role module energy cost and cooldown must be finite, non-negative values."); }
		return false;
	}
	return true;
}
