#pragma once

#include "CoreMinimal.h"
#include "PRDeveloperToolsTypes.generated.h"

UENUM(BlueprintType)
enum class EPRDeveloperAction : uint8
{
	CreateProfile,
	ActivateProfile,
	DeleteProfile,
	SaveCheckpoint,
	CreateLegacyV1Profile,
	PrepareShipRepairAcceptance,
	SpawnTestLoot,
	ToggleReady,
	StartMission,
	CorruptActivePrimary,
	FailNextSave
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDeveloperActionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Developer Tools") EPRDeveloperAction Action = EPRDeveloperAction::CreateProfile;
	UPROPERTY(BlueprintReadWrite, Category="Developer Tools") FString TextValue;
	UPROPERTY(BlueprintReadWrite, Category="Developer Tools") FGuid ProfileId;
	UPROPERTY(BlueprintReadWrite, Category="Developer Tools") bool bConfirmed = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDeveloperActionAvailability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") EPRDeveloperAction Action = EPRDeveloperAction::CreateProfile;
	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") bool bRequiresConfirmation = false;
	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") FString DisabledReason;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDeveloperActionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") EPRDeveloperAction Action = EPRDeveloperAction::CreateProfile;
	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category="Developer Tools") FString Diagnostic;
};

