#include "Production/PRProductionValidationReportWriter.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString ToSeverityString(const EPRProductionValidationSeverity Severity)
{
	switch (Severity)
	{
	case EPRProductionValidationSeverity::Info: return TEXT("Info");
	case EPRProductionValidationSeverity::Warning: return TEXT("Warning");
	default: return TEXT("Error");
	}
}

bool IsSafeSavedOutputDirectory(const FString& Directory)
{
	const FString AbsoluteDirectory = FPaths::ConvertRelativePathToFull(Directory);
	const FString SavedDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	return FPaths::IsUnderDirectory(AbsoluteDirectory, SavedDirectory) || AbsoluteDirectory.Equals(SavedDirectory, ESearchCase::IgnoreCase);
}
}

bool FPRProductionValidationReportWriter::Write(
	const FPRProductionValidationReport& Report,
	const FString& OutputDirectory,
	FPRProductionValidationReportPaths& OutPaths)
{
	OutPaths = FPRProductionValidationReportPaths();
	if (!IsSafeSavedOutputDirectory(OutputDirectory))
	{
		return false;
	}
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		return false;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("SchemaVersion"), 1);
	Root->SetStringField(TEXT("ProjectVersion"), Report.ProjectVersion);
	Root->SetNumberField(TEXT("ScannedAssetCount"), Report.ScannedAssetCount);
	Root->SetNumberField(TEXT("ErrorCount"), Report.ErrorCount);
	Root->SetNumberField(TEXT("WarningCount"), Report.WarningCount);
	Root->SetBoolField(TEXT("Passed"), Report.HasNoErrors());
	TArray<TSharedPtr<FJsonValue>> JsonIssues;
	for (const FPRProductionValidationIssue& Issue : Report.Issues)
	{
		const TSharedRef<FJsonObject> JsonIssue = MakeShared<FJsonObject>();
		JsonIssue->SetStringField(TEXT("RuleId"), Issue.RuleId);
		JsonIssue->SetStringField(TEXT("Severity"), ToSeverityString(Issue.Severity));
		JsonIssue->SetStringField(TEXT("AssetPath"), Issue.AssetPath);
		JsonIssue->SetStringField(TEXT("Message"), Issue.Message);
		JsonIssue->SetStringField(TEXT("Remediation"), Issue.Remediation);
		JsonIssues.Add(MakeShared<FJsonValueObject>(JsonIssue));
	}
	Root->SetArrayField(TEXT("Issues"), JsonIssues);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	FString Markdown;
	Markdown += TEXT("# ProjectRift Production Validation Report\n\n");
	Markdown += FString::Printf(TEXT("- Project version: `%s`\n"), *Report.ProjectVersion);
	Markdown += FString::Printf(TEXT("- Result: **%s**\n"), Report.HasNoErrors() ? TEXT("PASS") : TEXT("FAIL"));
	Markdown += FString::Printf(TEXT("- Scanned assets: %d\n- Errors: %d\n- Warnings: %d\n\n"), Report.ScannedAssetCount, Report.ErrorCount, Report.WarningCount);
	Markdown += TEXT("| Severity | Rule | Asset/Map | Message | Remediation |\n|---|---|---|---|---|\n");
	for (const FPRProductionValidationIssue& Issue : Report.Issues)
	{
		Markdown += FString::Printf(TEXT("| %s | `%s` | `%s` | %s | %s |\n"),
			*ToSeverityString(Issue.Severity), *Issue.RuleId, *Issue.AssetPath,
			*Issue.Message.Replace(TEXT("|"), TEXT("\\|")), *Issue.Remediation.Replace(TEXT("|"), TEXT("\\|")));
	}

	OutPaths.JsonPath = FPaths::Combine(OutputDirectory, TEXT("production-validation.json"));
	OutPaths.MarkdownPath = FPaths::Combine(OutputDirectory, TEXT("production-validation.md"));
	return FFileHelper::SaveStringToFile(JsonText, *OutPaths.JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& FFileHelper::SaveStringToFile(Markdown, *OutPaths.MarkdownPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
