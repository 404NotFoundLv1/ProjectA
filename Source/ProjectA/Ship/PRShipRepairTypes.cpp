#include "Ship/PRShipRepairTypes.h"

bool FPRShipRepairReceipt::IsValid(FString* OutDiagnostic) const
{
	if (!ProfileId.IsValid() || RepairProjectId.IsNone() || !TransactionId.IsValid())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair receipt identifiers are invalid."); }
		return false;
	}
	FPRProfileSnapshot Candidate;
	Candidate.ResourceWallet = SettledResourceWallet;
	Candidate.ShipModules = SettledShipModules;
	Candidate.Story = SettledStory;
	return Candidate.IsValid(OutDiagnostic);
}
