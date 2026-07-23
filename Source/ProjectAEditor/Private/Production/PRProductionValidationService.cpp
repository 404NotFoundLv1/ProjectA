#include "Production/PRProductionValidationService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Production/PRProductionPolicyLoader.h"
#include "Production/PRProductionLevelValidator.h"
#include "Sound/SoundBase.h"

namespace
{
void AddIssue(
	TArray<FPRProductionValidationIssue>& Issues,
	const TCHAR* RuleId,
	EPRProductionValidationSeverity Severity,
	const FString& AssetPath,
	const FString& Message,
	const FString& Remediation)
{
	FPRProductionValidationIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.RuleId = RuleId;
	Issue.Severity = Severity;
	Issue.AssetPath = AssetPath;
	Issue.Message = Message;
	Issue.Remediation = Remediation;
}

bool IsCoveredByRoot(const FString& PackageName, const FString& Root)
{
	return PackageName.Equals(Root, ESearchCase::CaseSensitive)
		|| PackageName.StartsWith(Root.EndsWith(TEXT("/")) ? Root : Root + TEXT("/"), ESearchCase::CaseSensitive);
}

bool IsProjectProductionPackage(const FString& PackageName, const FPRProductionPolicy& Policy)
{
	return Policy.ScanRoots.ContainsByPredicate([&PackageName](const FString& Root)
	{
		return IsCoveredByRoot(PackageName, Root);
	});
}

bool IsAllowedExternalDependency(const FString& Dependency, const FPRProductionPolicy& Policy)
{
	return Policy.AllowedExternalRoots.ContainsByPredicate([&Dependency](const FString& Root)
	{
		return IsCoveredByRoot(Dependency, Root);
	});
}

bool IsPowerOfTwo(const int32 Value)
{
	return Value > 0 && (Value & (Value - 1)) == 0;
}

bool HasAsciiName(const FString& Value)
{
	for (const TCHAR Character : Value)
	{
		if (Character < 32 || Character > 126)
		{
			return false;
		}
	}
	return true;
}

bool HasForbiddenGeneratedSuffix(const FString& AssetName)
{
	return AssetName.EndsWith(TEXT("_New"), ESearchCase::IgnoreCase)
		|| AssetName.EndsWith(TEXT("_Copy"), ESearchCase::IgnoreCase)
		|| AssetName.EndsWith(TEXT("_Duplicate"), ESearchCase::IgnoreCase);
}

FPRProductionAssetDescriptor MakeDescriptor(const FAssetData& AssetData, IAssetRegistry& Registry)
{
	FPRProductionAssetDescriptor Descriptor;
	Descriptor.PackageName = AssetData.PackageName.ToString();
	Descriptor.AssetName = AssetData.AssetName.ToString();
	Descriptor.ClassName = AssetData.AssetClassPath.GetAssetName().ToString();

	TArray<FName> Dependencies;
	Registry.GetDependencies(AssetData.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
	for (const FName Dependency : Dependencies)
	{
		Descriptor.Dependencies.Add(Dependency.ToString());
	}
	TArray<FName> Referencers;
	Registry.GetReferencers(AssetData.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
	Descriptor.bIsUnused = Referencers.IsEmpty();

	if (Descriptor.ClassName == TEXT("Texture2D"))
	{
		if (const UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
		{
			Descriptor.Width = Texture->GetSizeX();
			Descriptor.Height = Texture->GetSizeY();
		}
	}
	else if (Descriptor.ClassName == TEXT("StaticMesh"))
	{
		if (const UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset()))
		{
			Descriptor.bHasSimpleCollision = Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0;
			Descriptor.bHasNaniteOrLods = Mesh->GetNumLODs() > 1;
		}
	}
	else if (Descriptor.ClassName == TEXT("MaterialInstanceConstant"))
	{
		Descriptor.bIsMaterialInstance = true;
	}
	else if (Descriptor.ClassName == TEXT("SoundWave") || Descriptor.ClassName == TEXT("SoundCue"))
	{
		if (const USoundBase* Sound = Cast<USoundBase>(AssetData.GetAsset()))
		{
			Descriptor.bHasSoundClass = Sound->GetSoundClass() != nullptr;
		}
	}

	return Descriptor;
}

void CountIssues(FPRProductionValidationReport& Report)
{
	Report.ErrorCount = 0;
	Report.WarningCount = 0;
	for (const FPRProductionValidationIssue& Issue : Report.Issues)
	{
		if (Issue.Severity == EPRProductionValidationSeverity::Error)
		{
			++Report.ErrorCount;
		}
		else if (Issue.Severity == EPRProductionValidationSeverity::Warning)
		{
			++Report.WarningCount;
		}
	}
}
}

bool FPRProductionValidationService::HasErrors(const TArray<FPRProductionValidationIssue>& Issues)
{
	return Issues.ContainsByPredicate([](const FPRProductionValidationIssue& Issue)
	{
		return Issue.Severity == EPRProductionValidationSeverity::Error;
	});
}

TArray<FPRProductionValidationIssue> FPRProductionValidationService::ValidateLicenseEvidence(const FPRLicenseRegistry& Registry)
{
	TArray<FPRProductionValidationIssue> Issues;
	const FString ProjectDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	for (const FPRLicenseRecord& Record : Registry.Records)
	{
		const FString LicensePath = FPaths::ConvertRelativePathToFull(ProjectDirectory, Record.LicenseDocumentPath);
		if (Record.LicenseDocumentPath.IsEmpty() || !FPaths::IsUnderDirectory(LicensePath, ProjectDirectory) || !IFileManager::Get().FileExists(*LicensePath))
		{
			AddIssue(Issues, TEXT("PRP-LIC-003"), EPRProductionValidationSeverity::Error, Record.AssetPath,
				TEXT("Declared license evidence path does not resolve inside ProjectA."), TEXT("Retain the reviewed license declaration or document under ProjectA and update AssetLicenseRegistry.json."));
		}

		const bool bNeedsInvoice = !Record.SourceKind.Equals(TEXT("ProjectOwned"), ESearchCase::CaseSensitive)
			&& !Record.SourceKind.Equals(TEXT("EpicProvided"), ESearchCase::CaseSensitive);
		if (bNeedsInvoice)
		{
			const FString InvoicePath = FPaths::ConvertRelativePathToFull(ProjectDirectory, Record.InvoicePath);
			if (Record.InvoicePath.IsEmpty() || !FPaths::IsUnderDirectory(InvoicePath, ProjectDirectory) || !IFileManager::Get().FileExists(*InvoicePath))
			{
				AddIssue(Issues, TEXT("PRP-LIC-004"), EPRProductionValidationSeverity::Error, Record.AssetPath,
					TEXT("Third-party production source is missing a retained invoice or purchase record."), TEXT("Add the invoice path to AssetLicenseRegistry.json before production use."));
			}
		}
	}
	return Issues;
}

TArray<FPRProductionValidationIssue> FPRProductionValidationService::ValidateDescriptor(
	const FPRProductionAssetDescriptor& Descriptor,
	const FPRProductionPolicy& Policy,
	const FPRLicenseRegistry& Registry)
{
	TArray<FPRProductionValidationIssue> Issues;
	if (!IsProjectProductionPackage(Descriptor.PackageName, Policy))
	{
		return Issues;
	}

	const FPRProductionAssetRule* Rule = Policy.FindAssetRule(Descriptor.ClassName);
	if (Rule)
	{
		const bool bMatchesPrefix = Rule->AcceptedPrefixes.ContainsByPredicate([&Descriptor](const FString& AcceptedPrefix)
		{
			return Descriptor.AssetName.StartsWith(AcceptedPrefix, ESearchCase::CaseSensitive);
		});
		if (!bMatchesPrefix
			|| Descriptor.AssetName.Contains(TEXT(" ")) || !HasAsciiName(Descriptor.AssetName)
			|| HasForbiddenGeneratedSuffix(Descriptor.AssetName))
		{
			AddIssue(Issues, TEXT("PRP-NAM-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				FString::Printf(TEXT("Asset '%s' must use prefix '%s' and a stable ASCII name."), *Descriptor.AssetName, *Rule->Prefix),
				TEXT("Rename the asset using the published ProjectRift naming convention."));
		}
		if (Rule->MaxDimension > 0 && (Descriptor.Width > Rule->MaxDimension || Descriptor.Height > Rule->MaxDimension))
		{
			AddIssue(Issues, TEXT("PRP-TEX-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Texture exceeds the configured maximum dimension."), TEXT("Resize or classify the texture under an approved budget."));
		}
		if (Rule->bRequirePowerOfTwo && (!IsPowerOfTwo(Descriptor.Width) || !IsPowerOfTwo(Descriptor.Height)))
		{
			AddIssue(Issues, TEXT("PRP-TEX-002"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Texture dimensions must be powers of two."), TEXT("Re-export at a supported power-of-two resolution."));
		}
		if (Rule->bRequireSimpleCollision && !Descriptor.bHasSimpleCollision)
		{
			AddIssue(Issues, TEXT("PRP-MSH-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Static mesh is missing simple collision."), TEXT("Author simple collision before marking the mesh production-ready."));
		}
		if (Rule->bRequireNaniteOrLods && !Descriptor.bHasNaniteOrLods)
		{
			AddIssue(Issues, TEXT("PRP-MSH-002"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Static mesh has neither an acceptable LOD chain nor Nanite coverage."), TEXT("Enable Nanite or provide the configured LOD chain."));
		}
		if (Rule->bRequireMaterialInstance && !Descriptor.bIsMaterialInstance)
		{
			AddIssue(Issues, TEXT("PRP-MAT-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Production material use must resolve through a material instance."), TEXT("Create and reference an MI_ asset."));
		}
		if (Rule->bRequireSoundClass && !Descriptor.bHasSoundClass)
		{
			AddIssue(Issues, TEXT("PRP-AUD-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				TEXT("Audio asset has no configured sound class."), TEXT("Assign a ProjectRift sound class before production use."));
		}
	}

	if (!Registry.FindCoverage(Descriptor.PackageName))
	{
		AddIssue(Issues, TEXT("PRP-LIC-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
			TEXT("Production asset has no license registry coverage."), TEXT("Add an exact asset or reviewed folder declaration to AssetLicenseRegistry.json."));
	}

	for (const FString& Dependency : Descriptor.Dependencies)
	{
		if (Dependency.StartsWith(TEXT("/Game/ProjectRift/Developer/"), ESearchCase::CaseSensitive)
			|| Dependency.StartsWith(TEXT("/Game/ProjectRift/Tests/"), ESearchCase::CaseSensitive)
			|| Dependency.StartsWith(TEXT("/Game/Variant_"), ESearchCase::CaseSensitive))
		{
			AddIssue(Issues, TEXT("PRP-REF-001"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				FString::Printf(TEXT("Production asset hard-references forbidden content '%s'."), *Dependency), TEXT("Replace the dependency with ProjectRift production content."));
		}
		else if (Dependency.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive)
			&& !IsProjectProductionPackage(Dependency, Policy) && !IsAllowedExternalDependency(Dependency, Policy))
		{
			AddIssue(Issues, TEXT("PRP-REF-002"), EPRProductionValidationSeverity::Error, Descriptor.PackageName,
				FString::Printf(TEXT("Production asset references unapproved external content '%s'."), *Dependency), TEXT("Add a reviewed external root or replace the dependency."));
		}
	}

	if (Descriptor.bIsUnused)
	{
		AddIssue(Issues, TEXT("PRP-UNU-001"), EPRProductionValidationSeverity::Warning, Descriptor.PackageName,
			TEXT("Asset has no package referencers."), TEXT("Review the asset manually; the validator never deletes unused assets."));
	}

	return Issues;
}

FPRProductionValidationReport FPRProductionValidationService::Run(const FPRProductionValidationRequest& Request)
{
	FPRProductionValidationReport Report;
	FPRProductionPolicy Policy;
	FPRLicenseRegistry Registry;
	FPRProductionPolicyLoader::LoadProjectPolicy(Policy, Report.Issues);
	FPRProductionPolicyLoader::LoadProjectLicenseRegistry(Registry, Report.Issues);
	Report.Issues.Append(ValidateLicenseEvidence(Registry));
	Report.ProjectVersion = Policy.ProjectVersion;
	if (HasErrors(Report.Issues) || !Request.bValidateAssets)
	{
		CountIssues(Report);
		return Report;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	for (const FString& Root : Policy.ScanRoots)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(Root));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);
		for (const FAssetData& AssetData : Assets)
		{
			++Report.ScannedAssetCount;
			Report.Issues.Append(ValidateDescriptor(MakeDescriptor(AssetData, AssetRegistry), Policy, Registry));
		}
	}
	if (Request.bValidateLevels)
	{
		FPRProductionLevelValidator::Validate(Policy, Report.Issues);
	}
	Report.Issues.Sort([](const FPRProductionValidationIssue& Left, const FPRProductionValidationIssue& Right)
	{
		return Left.AssetPath == Right.AssetPath ? Left.RuleId < Right.RuleId : Left.AssetPath < Right.AssetPath;
	});
	CountIssues(Report);
	return Report;
}
