#pragma once

#include "Production/PRProductionValidationTypes.h"

struct FPRProductionAssetDescriptor
{
	FString PackageName;
	FString AssetName;
	FString ClassName;
	int32 Width = 0;
	int32 Height = 0;
	bool bHasSimpleCollision = true;
	bool bHasNaniteOrLods = true;
	bool bIsMaterialInstance = true;
	bool bHasSoundClass = true;
	bool bIsUnused = false;
	TArray<FString> Dependencies;
};

struct FPRProductionValidationRequest
{
	bool bValidateAssets = true;
	bool bValidateLevels = true;
};

struct FPRProductionValidationReport
{
	FString ProjectVersion;
	int32 ScannedAssetCount = 0;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	TArray<FPRProductionValidationIssue> Issues;

	bool HasNoErrors() const { return ErrorCount == 0; }
};

class PROJECTAEDITOR_API FPRProductionValidationService
{
public:
	static bool HasErrors(const TArray<FPRProductionValidationIssue>& Issues);
	static TArray<FPRProductionValidationIssue> ValidateLicenseEvidence(const FPRLicenseRegistry& Registry);
	static TArray<FPRProductionValidationIssue> ValidateDescriptor(
		const FPRProductionAssetDescriptor& Descriptor,
		const FPRProductionPolicy& Policy,
		const FPRLicenseRegistry& Registry);
	static FPRProductionValidationReport Run(const FPRProductionValidationRequest& Request = FPRProductionValidationRequest());
};
