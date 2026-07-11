#include "Settings/PRProjectSettings.h"

FName UPRProjectSettings::GetCategoryName() const
{
	return TEXT("ProjectRift");
}

FName UPRProjectSettings::GetSectionName() const
{
	return TEXT("Gameplay");
}
