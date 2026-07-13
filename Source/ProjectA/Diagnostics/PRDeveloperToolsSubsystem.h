#pragma once

#include "CoreMinimal.h"
#include "Diagnostics/PRDeveloperToolsTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PRDeveloperToolsSubsystem.generated.h"

class APlayerController;

UCLASS()
class PROJECTA_API UPRDeveloperToolsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintPure, Category="Developer Tools")
	FPRDeveloperActionAvailability GetActionAvailability(EPRDeveloperAction Action, APlayerController* LocalController) const;

	UFUNCTION(BlueprintCallable, Category="Developer Tools")
	FPRDeveloperActionResult ExecuteAction(const FPRDeveloperActionRequest& Request, APlayerController* LocalController);
};

