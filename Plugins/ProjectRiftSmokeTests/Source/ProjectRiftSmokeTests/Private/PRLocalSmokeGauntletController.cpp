#include "PRLocalSmokeGauntletController.h"

#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRPickupActor.h"
#include "Persistence/PRProfileTypes.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectRiftSmoke, Log, All);

namespace
{
const TCHAR* LexToString(const EPRLocalSmokeStage Stage)
{
	switch (Stage)
	{
	case EPRLocalSmokeStage::WaitingForLobby: return TEXT("WaitingForLobby");
	case EPRLocalSmokeStage::CreatingProfile: return TEXT("CreatingProfile");
	case EPRLocalSmokeStage::BindingProfile: return TEXT("BindingProfile");
	case EPRLocalSmokeStage::StartingMission: return TEXT("StartingMission");
	case EPRLocalSmokeStage::WaitingForRift: return TEXT("WaitingForRift");
	case EPRLocalSmokeStage::CollectingLoot: return TEXT("CollectingLoot");
	case EPRLocalSmokeStage::CompletingMission: return TEXT("CompletingMission");
	case EPRLocalSmokeStage::WaitingForSettlement: return TEXT("WaitingForSettlement");
	case EPRLocalSmokeStage::VerifyingReturn: return TEXT("VerifyingReturn");
	case EPRLocalSmokeStage::Passed: return TEXT("Passed");
	case EPRLocalSmokeStage::Failed: return TEXT("Failed");
	default: return TEXT("Unknown");
	}
}
}

bool UPRLocalSmokeGauntletController::IsAllowedTransition(
	const EPRLocalSmokeStage From,
	const EPRLocalSmokeStage To)
{
	if (From == To || From == EPRLocalSmokeStage::Passed || From == EPRLocalSmokeStage::Failed)
	{
		return false;
	}
	if (To == EPRLocalSmokeStage::Failed)
	{
		return true;
	}

	switch (From)
	{
	case EPRLocalSmokeStage::WaitingForLobby: return To == EPRLocalSmokeStage::CreatingProfile;
	case EPRLocalSmokeStage::CreatingProfile: return To == EPRLocalSmokeStage::BindingProfile;
	case EPRLocalSmokeStage::BindingProfile: return To == EPRLocalSmokeStage::StartingMission;
	case EPRLocalSmokeStage::StartingMission: return To == EPRLocalSmokeStage::WaitingForRift;
	case EPRLocalSmokeStage::WaitingForRift: return To == EPRLocalSmokeStage::CollectingLoot;
	case EPRLocalSmokeStage::CollectingLoot: return To == EPRLocalSmokeStage::CompletingMission;
	case EPRLocalSmokeStage::CompletingMission: return To == EPRLocalSmokeStage::WaitingForSettlement;
	case EPRLocalSmokeStage::WaitingForSettlement: return To == EPRLocalSmokeStage::VerifyingReturn;
	case EPRLocalSmokeStage::VerifyingReturn: return To == EPRLocalSmokeStage::Passed;
	default: return false;
	}
}

void UPRLocalSmokeGauntletController::OnInit()
{
	Super::OnInit();
	RunStartedAtSeconds = FPlatformTime::Seconds();
	StageStartedAtSeconds = RunStartedAtSeconds;
	LogStage(TEXT("Controller initialized"));
}

void UPRLocalSmokeGauntletController::OnPreMapChange()
{
	Super::OnPreMapChange();
	LogStage(TEXT("Map change starting"));
}

void UPRLocalSmokeGauntletController::OnPostMapChange(UWorld* World)
{
	Super::OnPostMapChange(World);
	LogStage(FString::Printf(TEXT("Map loaded: %s"), *GetNameSafe(World)));
}

void UPRLocalSmokeGauntletController::OnTick(const float TimeDelta)
{
	Super::OnTick(TimeDelta);
	if (bTestEnded)
	{
		return;
	}

	MarkHeartbeatActive(FString::Printf(TEXT("ProjectRift smoke stage: %s"), LexToString(CurrentStage)));
	const double Now = FPlatformTime::Seconds();
	if (Now - RunStartedAtSeconds > TotalTimeoutSeconds)
	{
		FailSmoke(TEXT("The ProjectRift smoke run exceeded the 180 second timeout."));
		return;
	}
	if (Now - StageStartedAtSeconds > StageTimeoutSeconds)
	{
		FailSmoke(FString::Printf(TEXT("Smoke stage %s exceeded the 30 second timeout."), LexToString(CurrentStage)));
		return;
	}

	TickCurrentStage();
}

APRPlayerController* UPRLocalSmokeGauntletController::ResolvePlayerController() const
{
	return Cast<APRPlayerController>(GetFirstPlayerController());
}

UPRSaveSubsystem* UPRLocalSmokeGauntletController::ResolveSaveSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UPRSaveSubsystem>() : nullptr;
}

bool UPRLocalSmokeGauntletController::TransitionTo(const EPRLocalSmokeStage NewStage)
{
	if (!IsAllowedTransition(CurrentStage, NewStage))
	{
		return false;
	}
	CurrentStage = NewStage;
	StageStartedAtSeconds = FPlatformTime::Seconds();
	bStageActionIssued = false;
	LogStage(TEXT("Stage entered"));
	return true;
}

void UPRLocalSmokeGauntletController::FailSmoke(const FString& Diagnostic)
{
	if (bTestEnded)
	{
		return;
	}
	const FString FailedFromStage = LexToString(CurrentStage);
	if (CurrentStage != EPRLocalSmokeStage::Failed)
	{
		TransitionTo(EPRLocalSmokeStage::Failed);
	}
	bTestEnded = true;
	UE_LOG(LogProjectRiftSmoke, Error, TEXT("PROJECTRIFT_SMOKE_RESULT=FAIL Stage=%s Diagnostic=%s"), *FailedFromStage, *Diagnostic);
	EndTest(1);
}

void UPRLocalSmokeGauntletController::PassSmoke()
{
	if (bTestEnded || !TransitionTo(EPRLocalSmokeStage::Passed))
	{
		return;
	}
	bTestEnded = true;
	UE_LOG(LogProjectRiftSmoke, Display, TEXT("PROJECTRIFT_SMOKE_RESULT=PASS ProfileId=%s SettlementId=%s"),
		*SmokeProfileId.ToString(EGuidFormats::DigitsWithHyphens),
		*SmokeSettlementId.ToString(EGuidFormats::DigitsWithHyphens));
	EndTest(0);
}

void UPRLocalSmokeGauntletController::TickCurrentStage()
{
	APRPlayerController* PlayerController = ResolvePlayerController();
	APRPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APRPlayerState>() : nullptr;
	UPRSaveSubsystem* SaveSubsystem = ResolveSaveSubsystem();
	UWorld* World = GetWorld();

	switch (CurrentStage)
	{
	case EPRLocalSmokeStage::WaitingForLobby:
		if (World && World->GetAuthGameMode<APRShipLobbyGameMode>() && PlayerController && PlayerState && SaveSubsystem)
		{
			TransitionTo(EPRLocalSmokeStage::CreatingProfile);
		}
		break;

	case EPRLocalSmokeStage::CreatingProfile:
		if (!SaveSubsystem || SaveSubsystem->IsGauntletSmokeInitializationRejected() || !SaveSubsystem->IsUsingIsolatedDevelopmentRoot())
		{
			FailSmoke(TEXT("Gauntlet save subsystem did not initialize with an isolated profile root."));
			break;
		}
		if (!SaveSubsystem->GetActiveProfileId().IsValid())
		{
			const FPRProfileOperationResult Result = SaveSubsystem->CreateProfile(TEXT("Gauntlet Smoke Profile"));
			if (!Result.IsSuccess())
			{
				FailSmoke(FString::Printf(TEXT("Could not create the isolated smoke profile: %s"), *Result.Diagnostic));
				break;
			}
		}
		SmokeProfileId = SaveSubsystem->GetActiveProfileId();
		if (!SmokeProfileId.IsValid())
		{
			FailSmoke(TEXT("Smoke profile identity is invalid after creation."));
			break;
		}
		TransitionTo(EPRLocalSmokeStage::BindingProfile);
		break;

	case EPRLocalSmokeStage::BindingProfile:
		if (!PlayerController || !PlayerState)
		{
			break;
		}
		if (PlayerState->IsMultiplayerProfileBound())
		{
			if (PlayerState->GetBoundProfileId() != SmokeProfileId)
			{
				FailSmoke(TEXT("Server bound a different profile GUID during smoke setup."));
				break;
			}
			TransitionTo(EPRLocalSmokeStage::StartingMission);
			break;
		}
		if (!bStageActionIssued)
		{
			bStageActionIssued = true;
			PlayerController->SubmitLocalMultiplayerProfile();
		}
		break;

	case EPRLocalSmokeStage::StartingMission:
		if (!PlayerController || !PlayerState || !World)
		{
			break;
		}
		if (!PlayerState->IsReady())
		{
			if (!bStageActionIssued)
			{
				bStageActionIssued = true;
				PlayerController->ServerSetReady(true);
			}
			break;
		}
		if (APRShipLobbyGameMode* LobbyGameMode = World->GetAuthGameMode<APRShipLobbyGameMode>())
		{
			if (!LobbyGameMode->CanStartRiftMission())
			{
				break;
			}
			if (!LobbyGameMode->StartRiftMission(PlayerController))
			{
				FailSmoke(TEXT("Authoritative lobby ServerTravel did not start."));
				break;
			}
			TransitionTo(EPRLocalSmokeStage::WaitingForRift);
		}
		break;

	case EPRLocalSmokeStage::WaitingForRift:
		if (APRRiftGameMode* RiftGameMode = World ? World->GetAuthGameMode<APRRiftGameMode>() : nullptr)
		{
			if (!RiftGameMode->HasRiftMissionStarted())
			{
				break;
			}
			SmokeSettlementId = RiftGameMode->GetCurrentSettlementId();
			if (RiftGameMode->GetMissionId() != FName(TEXT("Mission.Rift.Test.Hold"))
				|| !RiftGameMode->GetCurrentRunId().IsValid()
				|| !SmokeSettlementId.IsValid())
			{
				FailSmoke(TEXT("Rift mission identity is incomplete after ServerTravel."));
				break;
			}
			TransitionTo(EPRLocalSmokeStage::CollectingLoot);
		}
		break;

	case EPRLocalSmokeStage::CollectingLoot:
		if (!PlayerController || !PlayerState)
		{
			break;
		}
		if (!bStageActionIssued)
		{
			bStageActionIssued = true;
			APRPickupActor* Pickup = PlayerController->TrySpawnTestLootOnServer(70.0f);
			if (!Pickup || Pickup->GetItemInstance().ItemId != FName(TEXT("EnergyCrystal"))
				|| !PlayerController->TryPickupOnServer(Pickup))
			{
				FailSmoke(TEXT("Deterministic EnergyCrystal spawn or pickup failed."));
				break;
			}
			const UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
			if (!Inventory || Inventory->GetItemCount(TEXT("EnergyCrystal")) != 1)
			{
				FailSmoke(TEXT("Picked EnergyCrystal did not enter the runtime inventory exactly once."));
				break;
			}
			TransitionTo(EPRLocalSmokeStage::CompletingMission);
		}
		break;

	case EPRLocalSmokeStage::CompletingMission:
		if (APRRiftGameMode* RiftGameMode = World ? World->GetAuthGameMode<APRRiftGameMode>() : nullptr)
		{
			RiftGameMode->CompleteCurrentObjective();
			if (!RiftGameMode->RegisterPlayerExtracted(PlayerController))
			{
				FailSmoke(TEXT("Authoritative smoke extraction was rejected."));
				break;
			}
			TransitionTo(EPRLocalSmokeStage::WaitingForSettlement);
		}
		break;

	case EPRLocalSmokeStage::WaitingForSettlement:
		if (World && World->GetAuthGameMode<APRShipLobbyGameMode>())
		{
			TransitionTo(EPRLocalSmokeStage::VerifyingReturn);
		}
		break;

	case EPRLocalSmokeStage::VerifyingReturn:
		if (!SaveSubsystem || !PlayerState)
		{
			break;
		}
		{
			FPRProfileSnapshot Snapshot;
			if (!SaveSubsystem->GetActiveProfileSnapshot(Snapshot))
			{
				FailSmoke(TEXT("Returned lobby could not read the active smoke profile."));
				break;
			}
			const int32 SettlementOccurrences = Snapshot.ProcessedSettlementIds.FilterByPredicate(
				[this](const FGuid& SettlementId) { return SettlementId == SmokeSettlementId; }).Num();
			if (SaveSubsystem->GetActiveProfileId() != SmokeProfileId
				|| Snapshot.GetResourceCount(TEXT("EnergyCrystal")) != 1
				|| !Snapshot.Story.CompletedStoryNodeIds.Contains(TEXT("Story.Prologue.RiftTestHold"))
				|| SettlementOccurrences != 1
				|| SaveSubsystem->HasPendingSettlementReceipt()
				|| PlayerState->GetShipResourceCount(TEXT("EnergyCrystal")) != 1
				|| PlayerState->IsReady())
			{
				FailSmoke(TEXT("Returned lobby profile, settlement ledger, story, resources, or ready state is inconsistent."));
				break;
			}
		}
		PassSmoke();
		break;

	case EPRLocalSmokeStage::Passed:
	case EPRLocalSmokeStage::Failed:
	default:
		break;
	}
}

void UPRLocalSmokeGauntletController::LogStage(const FString& Detail) const
{
	UE_LOG(LogProjectRiftSmoke, Display, TEXT("PROJECTRIFT_SMOKE_STAGE=%s Detail=%s"), LexToString(CurrentStage), *Detail);
}
