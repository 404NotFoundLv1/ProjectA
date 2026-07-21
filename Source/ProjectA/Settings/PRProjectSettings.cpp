#include "Settings/PRProjectSettings.h"

UPRProjectSettings::UPRProjectSettings()
{
	RiftRiskBands = {
		{ EPRRiftRiskTier::Stable, 75.0f, 1.00f, 1.00f, 0.0f, 0.0f },
		{ EPRRiftRiskTier::Unstable, 50.0f, 1.15f, 1.10f, 0.0f, 0.0f },
		{ EPRRiftRiskTier::Critical, 25.0f, 1.35f, 1.25f, 2.0f, 12.0f },
		{ EPRRiftRiskTier::Collapse, 0.0f, 1.60f, 1.50f, 3.0f, 8.0f }
	};
}

FName UPRProjectSettings::GetCategoryName() const
{
	return TEXT("ProjectRift");
}

FName UPRProjectSettings::GetSectionName() const
{
	return TEXT("Gameplay");
}
