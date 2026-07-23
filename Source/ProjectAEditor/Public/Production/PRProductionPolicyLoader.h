#pragma once

#include "Production/PRProductionValidationTypes.h"

class PROJECTAEDITOR_API FPRProductionPolicyLoader
{
public:
	static FString GetDefaultPolicyPath();
	static FString GetDefaultLicenseRegistryPath();

	static bool LoadProjectPolicy(FPRProductionPolicy& OutPolicy, TArray<FPRProductionValidationIssue>& OutIssues);
	static bool LoadProjectLicenseRegistry(FPRLicenseRegistry& OutRegistry, TArray<FPRProductionValidationIssue>& OutIssues);
	static bool LoadPolicyFromJson(const FString& Json, FPRProductionPolicy& OutPolicy, TArray<FPRProductionValidationIssue>& OutIssues);
	static bool LoadLicenseRegistryFromJson(const FString& Json, FPRLicenseRegistry& OutRegistry, TArray<FPRProductionValidationIssue>& OutIssues);
};
