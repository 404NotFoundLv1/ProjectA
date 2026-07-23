#pragma once

#include "CoreMinimal.h"

enum class EPRProductionValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

struct FPRProductionValidationIssue
{
	FString RuleId;
	EPRProductionValidationSeverity Severity = EPRProductionValidationSeverity::Error;
	FString AssetPath;
	FString Message;
	FString Remediation;
};

struct FPRProductionAssetRule
{
	FString ClassName;
	FString Prefix;
	TArray<FString> AcceptedPrefixes;
	int32 MaxDimension = 0;
	bool bRequirePowerOfTwo = false;
	bool bRequireSimpleCollision = false;
	bool bRequireNaniteOrLods = false;
	bool bRequireMaterialInstance = false;
	bool bRequireSoundClass = false;
};

struct FPRProductionLevelContract
{
	FString MapPath;
	TArray<FName> RequiredSpawnGroupIds;
	TArray<FName> RequiredObjectiveNodeIds;
	bool bRequiresExtractionSocket = false;
	bool bRequiresBossArena = false;
	TArray<FName> RequiredStreamingPartitionIds;
};

struct FPRProductionPolicy
{
	int32 SchemaVersion = 0;
	FString ProjectVersion;
	TArray<FString> ScanRoots;
	TArray<FString> AllowedExternalRoots;
	TArray<FPRProductionAssetRule> AssetRules;
	TArray<FPRProductionLevelContract> LevelContracts;

	const FPRProductionAssetRule* FindAssetRule(const FString& ClassName) const;
};

enum class EPRLicensePermission : uint8
{
	Allowed,
	Denied,
	Unknown
};

struct FPRLicenseRecord
{
	FString AssetPath;
	bool bFolderCoverage = false;
	FString SourceKind;
	FString SourceUri;
	FString AuthorOrVendor;
	EPRLicensePermission CommercialUse = EPRLicensePermission::Unknown;
	EPRLicensePermission Modification = EPRLicensePermission::Unknown;
	EPRLicensePermission Attribution = EPRLicensePermission::Unknown;
	bool bAttributionRequired = false;
	FString AttributionText;
	FString LicenseDocumentPath;
	FString InvoicePath;
	FString ReviewedBy;
	FString ReviewedDate;
	FString Notes;
};

struct FPRLicenseRegistry
{
	int32 SchemaVersion = 0;
	TArray<FPRLicenseRecord> Records;

	const FPRLicenseRecord* FindCoverage(const FString& AssetPath) const;
};
