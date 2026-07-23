#include "Production/PRProductionValidationCommandlet.h"

#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Production/PRProductionValidationReportWriter.h"
#include "Production/PRProductionValidationService.h"

UPRProductionValidationCommandlet::UPRProductionValidationCommandlet()
{
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
}

int32 UPRProductionValidationCommandlet::Main(const FString& Params)
{
	FString OutputDirectory;
	FParse::Value(*Params, TEXT("ProjectRiftProductionReportDir="), OutputDirectory);
	if (OutputDirectory.IsEmpty())
	{
		OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ProjectRiftProductionValidation"));
	}
	else if (FPaths::IsRelative(OutputDirectory))
	{
		OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputDirectory);
	}

	const FPRProductionValidationReport Report = FPRProductionValidationService::Run();
	FPRProductionValidationReportPaths Paths;
	if (!FPRProductionValidationReportWriter::Write(Report, OutputDirectory, Paths))
	{
		UE_LOG(LogTemp, Error, TEXT("ProjectRift production validation could not write its report."));
		return 2;
	}
	UE_LOG(LogTemp, Display, TEXT("ProjectRift production validation: %s (errors=%d warnings=%d) JSON=%s"),
		Report.HasNoErrors() ? TEXT("PASS") : TEXT("FAIL"), Report.ErrorCount, Report.WarningCount, *Paths.JsonPath);
	return Report.HasNoErrors() ? 0 : 1;
}
