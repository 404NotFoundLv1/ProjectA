#pragma once

#include "Production/PRProductionValidationService.h"

struct FPRProductionValidationReportPaths
{
	FString JsonPath;
	FString MarkdownPath;
};

class PROJECTAEDITOR_API FPRProductionValidationReportWriter
{
public:
	static bool Write(
		const FPRProductionValidationReport& Report,
		const FString& OutputDirectory,
		FPRProductionValidationReportPaths& OutPaths);
};
