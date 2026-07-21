#include "Core/PREncounterExclusionVolume.h"

APREncounterExclusionVolume::APREncounterExclusionVolume()
{
	bReplicates = false;
	SetActorHiddenInGame(true);
}

bool APREncounterExclusionVolume::ExcludesLocation(const FVector& Location) const
{
	return EncompassesPoint(Location);
}
