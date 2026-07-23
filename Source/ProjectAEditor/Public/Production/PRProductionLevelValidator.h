#pragma once

#include "Production/PRProductionValidationTypes.h"

class PROJECTAEDITOR_API FPRProductionLevelValidator
{
public:
	static void Validate(const FPRProductionPolicy& Policy, TArray<FPRProductionValidationIssue>& OutIssues);
};
