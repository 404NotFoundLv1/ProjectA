#include "Roles/PREngineerModuleDataAsset.h"

#include "Deployables/PRDeployableActor.h"

bool UPREngineerModuleDataAsset::ValidateDefinition(FString* OutDiagnostic) const
{
	if (!Super::ValidateDefinition(OutDiagnostic))
	{
		return false;
	}
	if (DeployableKind == EPRDeployableKind::None || !DeployableActorClass)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Engineer modules require a deployable kind and actor class."); }
		return false;
	}
	if (!FMath::IsFinite(PlacementRange) || PlacementRange <= 0.0f
		|| !FMath::IsFinite(FootprintRadius) || FootprintRadius <= 0.0f
		|| !FMath::IsFinite(MaxSurfaceSlopeDegrees) || MaxSurfaceSlopeDegrees < 0.0f || MaxSurfaceSlopeDegrees > 90.0f
		|| !FMath::IsFinite(LifetimeSeconds) || LifetimeSeconds <= 0.0f
		|| !FMath::IsFinite(EffectRange) || EffectRange < 0.0f
		|| !FMath::IsFinite(EffectIntervalSeconds) || EffectIntervalSeconds <= 0.0f
		|| !FMath::IsFinite(PrimaryMagnitude) || PrimaryMagnitude < 0.0f)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Engineer deployment values must be finite and within their declared ranges."); }
		return false;
	}
	return true;
}
