#pragma once

#include "CoreMinimal.h"
#include "Diagnostics/PRDiagnosticsTypes.h"

PROJECTA_API bool PRRecordDiagnosticEvent(
	const UObject* WorldContextObject,
	EPRDiagnosticSeverity Severity,
	FName Category,
	FName EventCode,
	const FString& Message,
	const FPRDiagnosticContext& Context = FPRDiagnosticContext());
