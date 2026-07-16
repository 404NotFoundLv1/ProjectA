#include "Roles/PRRoleTypes.h"

bool FPRRoleLoadout::IsStructurallyValid(FString* OutDiagnostic) const
{
	if (Entries.Num() != 3)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("A role loadout must contain exactly Q, E, and R entries."); }
		return false;
	}

	TSet<EPRRoleModuleSlot> SeenSlots;
	TSet<FName> SeenModules;
	for (const FPRRoleModuleSlotEntry& Entry : Entries)
	{
		if (Entry.Slot == EPRRoleModuleSlot::None || Entry.ModuleId.IsNone()
			|| SeenSlots.Contains(Entry.Slot) || SeenModules.Contains(Entry.ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Role loadout contains a missing or duplicate slot/module entry."); }
			return false;
		}
		SeenSlots.Add(Entry.Slot);
		SeenModules.Add(Entry.ModuleId);
	}

	const bool bHasAllSlots = SeenSlots.Contains(EPRRoleModuleSlot::Q)
		&& SeenSlots.Contains(EPRRoleModuleSlot::E)
		&& SeenSlots.Contains(EPRRoleModuleSlot::R);
	if (!bHasAllSlots && OutDiagnostic)
	{
		*OutDiagnostic = TEXT("Role loadout must contain Q, E, and R exactly once.");
	}
	return bHasAllSlots;
}
