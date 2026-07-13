#pragma once

#include "CoreMinimal.h"
#include "Persistence/PRProfileTypes.h"

class APRPlayerState;

class PROJECTA_API FPRProfileRuntimeBridge
{
public:
	static bool CaptureFromPlayerState(const APRPlayerState* PlayerState, FPRProfileSnapshot& InOutSnapshot, FString& OutDiagnostic);
	static bool ApplyToPlayerState(const FPRProfileSnapshot& Snapshot, APRPlayerState* PlayerState, FString& OutDiagnostic);
};
