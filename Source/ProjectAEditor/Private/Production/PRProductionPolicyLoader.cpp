#include "Production/PRProductionPolicyLoader.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 ExpectedSchemaVersion = 1;

void AddIssue(
	TArray<FPRProductionValidationIssue>& Issues,
	const TCHAR* RuleId,
	const FString& Message,
	const FString& AssetPath = FString(),
	const FString& Remediation = FString())
{
	FPRProductionValidationIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.RuleId = RuleId;
	Issue.Severity = EPRProductionValidationSeverity::Error;
	Issue.AssetPath = AssetPath;
	Issue.Message = Message;
	Issue.Remediation = Remediation;
}

bool ParseRootObject(const FString& Json, TSharedPtr<FJsonObject>& OutRoot, TArray<FPRProductionValidationIssue>& OutIssues, const TCHAR* RuleId)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
	{
		AddIssue(OutIssues, RuleId, TEXT("JSON is malformed or does not contain an object."));
		return false;
	}
	return true;
}

bool ReadRequiredString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& OutValue, TArray<FPRProductionValidationIssue>& OutIssues, const TCHAR* RuleId)
{
	if (!Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
	{
		AddIssue(OutIssues, RuleId, FString::Printf(TEXT("Required string field '%s' is missing."), Field));
		return false;
	}
	return true;
}

bool ParsePermission(const FString& Source, EPRLicensePermission& OutPermission)
{
	if (Source.Equals(TEXT("Allowed"), ESearchCase::CaseSensitive))
	{
		OutPermission = EPRLicensePermission::Allowed;
		return true;
	}
	if (Source.Equals(TEXT("Denied"), ESearchCase::CaseSensitive))
	{
		OutPermission = EPRLicensePermission::Denied;
		return true;
	}
	if (Source.Equals(TEXT("Unknown"), ESearchCase::CaseSensitive))
	{
		OutPermission = EPRLicensePermission::Unknown;
		return true;
	}
	return false;
}

bool ReadNameArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FName>& OutValues, TArray<FPRProductionValidationIssue>& OutIssues)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values))
	{
		return true;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString StringValue;
		if (!Value.IsValid() || !Value->TryGetString(StringValue) || StringValue.IsEmpty())
		{
			AddIssue(OutIssues, TEXT("PRP-POL-001"), FString::Printf(TEXT("'%s' contains an empty identifier."), Field));
			return false;
		}
		OutValues.Add(FName(StringValue));
	}
	return true;
}

bool HasErrors(const TArray<FPRProductionValidationIssue>& Issues)
{
	return Issues.ContainsByPredicate([](const FPRProductionValidationIssue& Issue)
	{
		return Issue.Severity == EPRProductionValidationSeverity::Error;
	});
}
}

FString FPRProductionPolicyLoader::GetDefaultPolicyPath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("ProjectRift/ProductionValidation.json"));
}

FString FPRProductionPolicyLoader::GetDefaultLicenseRegistryPath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("ProjectRift/AssetLicenseRegistry.json"));
}

bool FPRProductionPolicyLoader::LoadProjectPolicy(FPRProductionPolicy& OutPolicy, TArray<FPRProductionValidationIssue>& OutIssues)
{
	FString Json;
	const FString Path = GetDefaultPolicyPath();
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("Production policy file is missing."), Path);
		return false;
	}
	return LoadPolicyFromJson(Json, OutPolicy, OutIssues);
}

bool FPRProductionPolicyLoader::LoadProjectLicenseRegistry(FPRLicenseRegistry& OutRegistry, TArray<FPRProductionValidationIssue>& OutIssues)
{
	FString Json;
	const FString Path = GetDefaultLicenseRegistryPath();
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		AddIssue(OutIssues, TEXT("PRP-LIC-001"), TEXT("Asset license registry file is missing."), Path);
		return false;
	}
	return LoadLicenseRegistryFromJson(Json, OutRegistry, OutIssues);
}

bool FPRProductionPolicyLoader::LoadPolicyFromJson(const FString& Json, FPRProductionPolicy& OutPolicy, TArray<FPRProductionValidationIssue>& OutIssues)
{
	OutPolicy = FPRProductionPolicy();
	TSharedPtr<FJsonObject> Root;
	if (!ParseRootObject(Json, Root, OutIssues, TEXT("PRP-POL-001")))
	{
		return false;
	}
	if (!Root->TryGetNumberField(TEXT("SchemaVersion"), OutPolicy.SchemaVersion) || OutPolicy.SchemaVersion != ExpectedSchemaVersion)
	{
		AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("Production policy SchemaVersion must be 1."));
	}
	ReadRequiredString(Root, TEXT("ProjectVersion"), OutPolicy.ProjectVersion, OutIssues, TEXT("PRP-POL-001"));

	const TArray<TSharedPtr<FJsonValue>>* ScanRoots = nullptr;
	if (!Root->TryGetArrayField(TEXT("ScanRoots"), ScanRoots) || ScanRoots->IsEmpty())
	{
		AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("Production policy requires at least one ScanRoots entry."));
	}
	else
	{
		for (const TSharedPtr<FJsonValue>& Value : *ScanRoots)
		{
			FString RootPath;
			if (!Value.IsValid() || !Value->TryGetString(RootPath) || !RootPath.StartsWith(TEXT("/Game/")))
			{
				AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("ScanRoots entries must begin with /Game/."));
				continue;
			}
			OutPolicy.ScanRoots.Add(RootPath);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ExternalRoots = nullptr;
	if (Root->TryGetArrayField(TEXT("AllowedExternalRoots"), ExternalRoots))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ExternalRoots)
		{
			FString RootPath;
			if (!Value.IsValid() || !Value->TryGetString(RootPath) || !RootPath.StartsWith(TEXT("/")))
			{
				AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("AllowedExternalRoots entries must be package roots."));
				continue;
			}
			OutPolicy.AllowedExternalRoots.Add(RootPath);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetRules = nullptr;
	if (!Root->TryGetArrayField(TEXT("AssetRules"), AssetRules))
	{
		AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("Production policy requires AssetRules."));
	}
	else
	{
		for (const TSharedPtr<FJsonValue>& Value : *AssetRules)
		{
			const TSharedPtr<FJsonObject>* RuleObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(RuleObject) || !RuleObject || !RuleObject->IsValid())
			{
				AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("AssetRules entries must be objects."));
				continue;
			}
			FPRProductionAssetRule Rule;
			ReadRequiredString(*RuleObject, TEXT("ClassName"), Rule.ClassName, OutIssues, TEXT("PRP-POL-001"));
			ReadRequiredString(*RuleObject, TEXT("Prefix"), Rule.Prefix, OutIssues, TEXT("PRP-POL-001"));
			const TArray<TSharedPtr<FJsonValue>>* AcceptedPrefixes = nullptr;
			if ((*RuleObject)->TryGetArrayField(TEXT("AcceptedPrefixes"), AcceptedPrefixes))
			{
				for (const TSharedPtr<FJsonValue>& PrefixValue : *AcceptedPrefixes)
				{
					FString AcceptedPrefix;
					if (PrefixValue.IsValid() && PrefixValue->TryGetString(AcceptedPrefix) && !AcceptedPrefix.IsEmpty())
					{
						Rule.AcceptedPrefixes.Add(AcceptedPrefix);
					}
					else
					{
						AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("AcceptedPrefixes entries must be non-empty strings."));
					}
				}
			}
			if (Rule.AcceptedPrefixes.IsEmpty())
			{
				Rule.AcceptedPrefixes.Add(Rule.Prefix);
			}
			(*RuleObject)->TryGetNumberField(TEXT("MaxDimension"), Rule.MaxDimension);
			(*RuleObject)->TryGetBoolField(TEXT("bRequirePowerOfTwo"), Rule.bRequirePowerOfTwo);
			(*RuleObject)->TryGetBoolField(TEXT("bRequireSimpleCollision"), Rule.bRequireSimpleCollision);
			(*RuleObject)->TryGetBoolField(TEXT("bRequireNaniteOrLods"), Rule.bRequireNaniteOrLods);
			(*RuleObject)->TryGetBoolField(TEXT("bRequireMaterialInstance"), Rule.bRequireMaterialInstance);
			(*RuleObject)->TryGetBoolField(TEXT("bRequireSoundClass"), Rule.bRequireSoundClass);
			if (!Rule.ClassName.IsEmpty() && !Rule.Prefix.IsEmpty())
			{
				if (OutPolicy.FindAssetRule(Rule.ClassName))
				{
					AddIssue(OutIssues, TEXT("PRP-POL-001"), FString::Printf(TEXT("Asset rule for '%s' is duplicated."), *Rule.ClassName));
				}
				else
				{
					OutPolicy.AssetRules.Add(MoveTemp(Rule));
				}
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* LevelContracts = nullptr;
	if (Root->TryGetArrayField(TEXT("LevelContracts"), LevelContracts))
	{
		for (const TSharedPtr<FJsonValue>& Value : *LevelContracts)
		{
			const TSharedPtr<FJsonObject>* ContractObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(ContractObject) || !ContractObject || !ContractObject->IsValid())
			{
				AddIssue(OutIssues, TEXT("PRP-POL-001"), TEXT("LevelContracts entries must be objects."));
				continue;
			}
			FPRProductionLevelContract Contract;
			ReadRequiredString(*ContractObject, TEXT("MapPath"), Contract.MapPath, OutIssues, TEXT("PRP-POL-001"));
			ReadNameArray(*ContractObject, TEXT("RequiredSpawnGroupIds"), Contract.RequiredSpawnGroupIds, OutIssues);
			ReadNameArray(*ContractObject, TEXT("RequiredObjectiveNodeIds"), Contract.RequiredObjectiveNodeIds, OutIssues);
			ReadNameArray(*ContractObject, TEXT("RequiredStreamingPartitionIds"), Contract.RequiredStreamingPartitionIds, OutIssues);
			(*ContractObject)->TryGetBoolField(TEXT("bRequiresExtractionSocket"), Contract.bRequiresExtractionSocket);
			(*ContractObject)->TryGetBoolField(TEXT("bRequiresBossArena"), Contract.bRequiresBossArena);
			if (!Contract.MapPath.IsEmpty())
			{
				OutPolicy.LevelContracts.Add(MoveTemp(Contract));
			}
		}
	}

	return !HasErrors(OutIssues);
}

bool FPRProductionPolicyLoader::LoadLicenseRegistryFromJson(const FString& Json, FPRLicenseRegistry& OutRegistry, TArray<FPRProductionValidationIssue>& OutIssues)
{
	OutRegistry = FPRLicenseRegistry();
	TSharedPtr<FJsonObject> Root;
	if (!ParseRootObject(Json, Root, OutIssues, TEXT("PRP-LIC-001")))
	{
		return false;
	}
	if (!Root->TryGetNumberField(TEXT("SchemaVersion"), OutRegistry.SchemaVersion) || OutRegistry.SchemaVersion != ExpectedSchemaVersion)
	{
		AddIssue(OutIssues, TEXT("PRP-LIC-001"), TEXT("Asset license registry SchemaVersion must be 1."));
	}
	const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
	if (!Root->TryGetArrayField(TEXT("Records"), Records))
	{
		AddIssue(OutIssues, TEXT("PRP-LIC-001"), TEXT("Asset license registry requires Records."));
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Records)
	{
		const TSharedPtr<FJsonObject>* RecordObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(RecordObject) || !RecordObject || !RecordObject->IsValid())
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-001"), TEXT("License Records entries must be objects."));
			continue;
		}
		FPRLicenseRecord Record;
		ReadRequiredString(*RecordObject, TEXT("AssetPath"), Record.AssetPath, OutIssues, TEXT("PRP-LIC-001"));
		ReadRequiredString(*RecordObject, TEXT("SourceKind"), Record.SourceKind, OutIssues, TEXT("PRP-LIC-001"));
		(*RecordObject)->TryGetBoolField(TEXT("bFolderCoverage"), Record.bFolderCoverage);
		(*RecordObject)->TryGetStringField(TEXT("SourceUri"), Record.SourceUri);
		(*RecordObject)->TryGetStringField(TEXT("AuthorOrVendor"), Record.AuthorOrVendor);
		(*RecordObject)->TryGetStringField(TEXT("AttributionText"), Record.AttributionText);
		(*RecordObject)->TryGetStringField(TEXT("LicenseDocumentPath"), Record.LicenseDocumentPath);
		(*RecordObject)->TryGetStringField(TEXT("InvoicePath"), Record.InvoicePath);
		(*RecordObject)->TryGetStringField(TEXT("ReviewedBy"), Record.ReviewedBy);
		(*RecordObject)->TryGetStringField(TEXT("ReviewedDate"), Record.ReviewedDate);
		(*RecordObject)->TryGetStringField(TEXT("Notes"), Record.Notes);

		FString PermissionText;
		const auto ParseRequiredPermission = [&](const TCHAR* Field, EPRLicensePermission& Permission)
		{
			if (!(*RecordObject)->TryGetStringField(Field, PermissionText) || !ParsePermission(PermissionText, Permission))
			{
				AddIssue(OutIssues, TEXT("PRP-LIC-002"), FString::Printf(TEXT("License record '%s' has an invalid '%s' permission."), *Record.AssetPath, Field), Record.AssetPath);
				return false;
			}
			if (Permission == EPRLicensePermission::Unknown)
			{
				AddIssue(OutIssues, TEXT("PRP-LIC-002"), FString::Printf(TEXT("License record '%s' leaves '%s' unknown."), *Record.AssetPath, Field), Record.AssetPath);
				return false;
			}
			return true;
		};
		ParseRequiredPermission(TEXT("CommercialUse"), Record.CommercialUse);
		ParseRequiredPermission(TEXT("Modification"), Record.Modification);
		if (!(*RecordObject)->TryGetStringField(TEXT("Attribution"), PermissionText))
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-002"), FString::Printf(TEXT("License record '%s' is missing Attribution."), *Record.AssetPath), Record.AssetPath);
		}
		else if (PermissionText.Equals(TEXT("Required"), ESearchCase::CaseSensitive))
		{
			Record.Attribution = EPRLicensePermission::Allowed;
			Record.bAttributionRequired = true;
		}
		else if (PermissionText.Equals(TEXT("NotRequired"), ESearchCase::CaseSensitive))
		{
			Record.Attribution = EPRLicensePermission::Allowed;
		}
		else if (PermissionText.Equals(TEXT("Denied"), ESearchCase::CaseSensitive))
		{
			Record.Attribution = EPRLicensePermission::Denied;
		}
		else
		{
			Record.Attribution = EPRLicensePermission::Unknown;
			AddIssue(OutIssues, TEXT("PRP-LIC-002"), FString::Printf(TEXT("License record '%s' leaves Attribution unknown or invalid."), *Record.AssetPath), Record.AssetPath);
		}
		if (Record.bAttributionRequired && Record.AttributionText.IsEmpty())
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-002"), FString::Printf(TEXT("License record '%s' requires attribution text."), *Record.AssetPath), Record.AssetPath);
		}
		if (Record.LicenseDocumentPath.IsEmpty())
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-003"), FString::Printf(TEXT("License record '%s' is missing LicenseDocumentPath."), *Record.AssetPath), Record.AssetPath);
		}
		if (!Record.AssetPath.StartsWith(TEXT("/Game/")))
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-001"), TEXT("License AssetPath must begin with /Game/."), Record.AssetPath);
		}
		const bool bDuplicate = OutRegistry.Records.ContainsByPredicate([&Record](const FPRLicenseRecord& Existing)
		{
			return Existing.AssetPath.Equals(Record.AssetPath, ESearchCase::CaseSensitive) && Existing.bFolderCoverage == Record.bFolderCoverage;
		});
		if (bDuplicate)
		{
			AddIssue(OutIssues, TEXT("PRP-LIC-001"), FString::Printf(TEXT("License record '%s' is duplicated."), *Record.AssetPath), Record.AssetPath);
		}
		else if (!Record.AssetPath.IsEmpty())
		{
			OutRegistry.Records.Add(MoveTemp(Record));
		}
	}
	return !HasErrors(OutIssues);
}
