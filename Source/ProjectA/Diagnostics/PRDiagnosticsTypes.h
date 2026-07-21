#pragma once

#include "CoreMinimal.h"
#include "Core/PREncounterDirectorTypes.h"
#include "PRDiagnosticsTypes.generated.h"

UENUM(BlueprintType)
enum class EPRDiagnosticSeverity : uint8
{
	Verbose,
	Info,
	Warning,
	Error,
	Critical
};

UENUM(BlueprintType)
enum class EPRDiagnosticHealth : uint8
{
	Healthy,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FName MapName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FName NetMode = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FGuid ProfileId;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FGuid RunId;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FGuid SettlementId;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FGuid TransactionId;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	int64 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FDateTime TimestampUtc;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	EPRDiagnosticSeverity Severity = EPRDiagnosticSeverity::Info;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FName Category = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FName EventCode = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics")
	FPRDiagnosticContext Context;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticFilter
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	EPRDiagnosticSeverity MinimumSeverity = EPRDiagnosticSeverity::Info;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	TArray<FName> Categories;

	UPROPERTY(BlueprintReadWrite, Category="Diagnostics")
	FString SearchText;

	bool Matches(const FPRDiagnosticEvent& Event) const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticEnvironmentSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ProjectVersion;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString EngineVersion;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString BuildConfiguration;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString PlatformName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString MapName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString WorldType;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString NetMode;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 PIEInstance = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") double UptimeSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float AverageFps = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float AverageFrameTimeMs = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ControllerName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString PawnName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString PlayerStateName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FGuid ActiveProfileId;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FGuid BoundProfileId;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ActiveProfileDisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString LastSaveStatus;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bPendingSettlement = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bPendingRepair = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FName SelectedRoleId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float Health = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float MaxHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float Shield = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float MaxShield = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float Energy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float MaxEnergy = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bDowned = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString InventorySummary;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ResourceSummary;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FName CurrentChapterId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ShipModuleSummary;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticTeamPlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 PlayerId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FGuid BoundProfileId;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bProfileBound = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bReady = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FName SelectedRoleId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bRepairPending = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticTeamSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString SessionState;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") TArray<FPRDiagnosticTeamPlayerSnapshot> Players;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FName SelectedMissionId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bTeamMissionReady = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString StartBlockReason;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticRiftSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FName MissionId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FGuid RunId;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FGuid SettlementId;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString ObjectiveState;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float Stability = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float MissionTime = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") float ObjectiveProgress = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 AlivePlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 ExtractedPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 KilledEnemies = 0;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bSettlementReady = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString SettlementStatus;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FPREncounterDirectorSnapshot EncounterDirector;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FDateTime GeneratedAtUtc;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FPRDiagnosticEnvironmentSnapshot Environment;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FPRDiagnosticPlayerSnapshot Player;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FPRDiagnosticTeamSnapshot Team;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FPRDiagnosticRiftSnapshot Rift;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") EPRDiagnosticHealth Health = EPRDiagnosticHealth::Healthy;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") TArray<FString> HealthMessages;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRDiagnosticExportResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString OutputPath;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") int32 EventCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Diagnostics") FString Diagnostic;
};
