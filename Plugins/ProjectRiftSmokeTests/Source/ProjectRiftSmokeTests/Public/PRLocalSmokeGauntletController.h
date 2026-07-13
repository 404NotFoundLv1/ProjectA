#pragma once

#include "CoreMinimal.h"
#include "GauntletTestController.h"
#include "PRLocalSmokeGauntletController.generated.h"

class APRPlayerController;
class UPRSaveSubsystem;
class UWorld;

UENUM()
enum class EPRLocalSmokeStage : uint8
{
	WaitingForLobby,
	CreatingProfile,
	BindingProfile,
	StartingMission,
	WaitingForRift,
	CollectingLoot,
	CompletingMission,
	WaitingForSettlement,
	VerifyingReturn,
	Passed,
	Failed
};

UCLASS()
class PROJECTRIFTSMOKETESTS_API UPRLocalSmokeGauntletController : public UGauntletTestController
{
	GENERATED_BODY()

public:
	static constexpr float StageTimeoutSeconds = 30.0f;
	static constexpr float TotalTimeoutSeconds = 180.0f;

	static bool IsAllowedTransition(EPRLocalSmokeStage From, EPRLocalSmokeStage To);

	virtual void OnInit() override;
	virtual void OnPreMapChange() override;
	virtual void OnPostMapChange(UWorld* World) override;
	virtual void OnTick(float TimeDelta) override;

private:
	APRPlayerController* ResolvePlayerController() const;
	UPRSaveSubsystem* ResolveSaveSubsystem() const;
	bool TransitionTo(EPRLocalSmokeStage NewStage);
	void FailSmoke(const FString& Diagnostic);
	void PassSmoke();
	void TickCurrentStage();
	void LogStage(const FString& Detail) const;

	EPRLocalSmokeStage CurrentStage = EPRLocalSmokeStage::WaitingForLobby;
	double RunStartedAtSeconds = 0.0;
	double StageStartedAtSeconds = 0.0;
	bool bStageActionIssued = false;
	bool bTestEnded = false;
	FGuid SmokeProfileId;
	FGuid SmokeSettlementId;
};
