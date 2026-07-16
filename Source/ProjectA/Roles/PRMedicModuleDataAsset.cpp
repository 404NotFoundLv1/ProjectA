#include "Roles/PRMedicModuleDataAsset.h"

bool UPRMedicModuleDataAsset::ValidateDefinition(FString* OutDiagnostic) const
{
	if (!Super::ValidateDefinition(OutDiagnostic))
	{
		return false;
	}

	if (MedicKind == EPRMedicModuleKind::None
		|| !FMath::IsFinite(PrimaryMagnitude) || PrimaryMagnitude < 0.0f
		|| !FMath::IsFinite(TargetRange) || TargetRange < 0.0f
		|| !FMath::IsFinite(EffectRadius) || EffectRadius < 0.0f
		|| !FMath::IsFinite(DurationSeconds) || DurationSeconds < 0.0f)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Medic modules require a kind and finite non-negative parameters."); }
		return false;
	}

	switch (MedicKind)
	{
	case EPRMedicModuleKind::FieldHeal:
		if (PrimaryMagnitude > 0.0f && TargetRange > 0.0f)
		{
			return true;
		}
		break;
	case EPRMedicModuleKind::PurificationPulse:
		if (EffectRadius > 0.0f)
		{
			return true;
		}
		break;
	case EPRMedicModuleKind::ReconScan:
		if (EffectRadius > 0.0f && DurationSeconds > 0.0f)
		{
			return true;
		}
		break;
	default:
		break;
	}

	if (OutDiagnostic) { *OutDiagnostic = TEXT("Medic module parameters do not satisfy the selected module kind."); }
	return false;
}
