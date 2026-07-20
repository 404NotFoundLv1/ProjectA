#include "Crafting/PRCraftingTypes.h"

bool FPRCraftingReceipt::IsValid(FString* OutDiagnostic) const
{
	if (!ProfileId.IsValid() || !TransactionId.IsValid() || (Operation == EPRCraftingOperation::Craft && RecipeId.IsNone())
		|| ((Operation == EPRCraftingOperation::Dismantle || Operation == EPRCraftingOperation::Upgrade) && !TargetInstanceGuid.IsValid()))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Crafting receipt identifiers are invalid."); }
		return false;
	}
	FPRProfileSnapshot Candidate;
	Candidate.ResourceWallet = SettledResourceWallet;
	Candidate.BackpackItems = SettledBackpackItems;
	Candidate.Equipment = SettledEquipment;
	return Candidate.IsValid(OutDiagnostic);
}
