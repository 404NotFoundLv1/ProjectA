#pragma once

#include "CoreMinimal.h"
#include "Diagnostics/PRDiagnosticsTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PRDiagnosticsSubsystem.generated.h"

class APlayerController;

UCLASS()
class PROJECTA_API UPRDiagnosticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	bool RecordEvent(EPRDiagnosticSeverity Severity, FName Category, FName EventCode, const FString& Message, const FPRDiagnosticContext& Context);

	UFUNCTION(BlueprintPure, Category="Diagnostics")
	TArray<FPRDiagnosticEvent> GetEvents(const FPRDiagnosticFilter& Filter) const;

	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void ClearEvents();

	UFUNCTION(BlueprintPure, Category="Diagnostics")
	FPRDiagnosticSnapshot BuildSnapshot(APlayerController* LocalController) const;

	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	FPRDiagnosticExportResult ExportSnapshot(APlayerController* LocalController);

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation(const FString& InExportRoot);
#endif

private:
	mutable FCriticalSection EventMutex;
	TArray<FPRDiagnosticEvent> Events;
	int64 NextSequence = 1;
	int32 MaxEventCount = 500;
	FString ExportRootOverride;
};
