#include "Core/PRRiftGameState.h"

#include "Net/UnrealNetwork.h"

APRRiftGameState::APRRiftGameState()
{
}

void APRRiftGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRRiftGameState, CurrentObjectiveState);
	DOREPLIFETIME(APRRiftGameState, RiftStability);
	DOREPLIFETIME(APRRiftGameState, bExtractionAvailable);
	DOREPLIFETIME(APRRiftGameState, ExtractedPlayerCount);
	DOREPLIFETIME(APRRiftGameState, bExtractionComplete);
	DOREPLIFETIME(APRRiftGameState, AlivePlayerCount);
	DOREPLIFETIME(APRRiftGameState, DifficultyPlayerCount);
	DOREPLIFETIME(APRRiftGameState, MissionTime);
	DOREPLIFETIME(APRRiftGameState, ObjectiveProgress);
	DOREPLIFETIME(APRRiftGameState, KilledEnemyCount);

	DOREPLIFETIME(APRRiftGameState, ObjectiveSummaries);
	DOREPLIFETIME(APRRiftGameState, RiftRiskSnapshot);
	DOREPLIFETIME(APRRiftGameState, MissionRuleSnapshot);
	DOREPLIFETIME(APRRiftGameState, MissionModifierSummaries);
	DOREPLIFETIME(APRRiftGameState, SettlementData);
	DOREPLIFETIME(APRRiftGameState, bSettlementReady);
}

void APRRiftGameState::SetCurrentObjectiveState(const EPRRiftObjectiveState InCurrentObjectiveState)
{
	CurrentObjectiveState = InCurrentObjectiveState;
}

void APRRiftGameState::SetRiftStability(const float InRiftStability)
{
	RiftStability = FMath::Clamp(InRiftStability, 0.0f, 100.0f);
}

void APRRiftGameState::SetExtractionAvailable(const bool bInExtractionAvailable)
{
	bExtractionAvailable = bInExtractionAvailable;
}

void APRRiftGameState::SetExtractedPlayerCount(const int32 InExtractedPlayerCount)
{
	ExtractedPlayerCount = FMath::Max(0, InExtractedPlayerCount);
}

void APRRiftGameState::SetExtractionComplete(const bool bInExtractionComplete)
{
	bExtractionComplete = bInExtractionComplete;
}

void APRRiftGameState::SetAlivePlayerCount(const int32 InAlivePlayerCount)
{
	AlivePlayerCount = FMath::Max(0, InAlivePlayerCount);
}

void APRRiftGameState::SetDifficultyPlayerCount(const int32 InDifficultyPlayerCount)
{
	DifficultyPlayerCount = FMath::Clamp(InDifficultyPlayerCount, 1, 4);
}

void APRRiftGameState::SetMissionTime(const float InMissionTime)
{
	MissionTime = FMath::Max(0.0f, InMissionTime);
}

void APRRiftGameState::SetObjectiveProgress(const float InObjectiveProgress)
{
	ObjectiveProgress = FMath::Clamp(InObjectiveProgress, 0.0f, 1.0f);
}

void APRRiftGameState::SetKilledEnemyCount(const int32 InKilledEnemyCount)
{
	KilledEnemyCount = FMath::Max(0, InKilledEnemyCount);
}

void APRRiftGameState::IncrementKilledEnemyCount()
{
	SetKilledEnemyCount(KilledEnemyCount + 1);
}

void APRRiftGameState::SetObjectiveSummaries(const TArray<FPRObjectiveSummary>& InObjectiveSummaries)
{
	ObjectiveSummaries = InObjectiveSummaries;
}

void APRRiftGameState::SetRiftRiskSnapshot(const FPRRiftRiskSnapshot& InRiskSnapshot)
{
	RiftRiskSnapshot = InRiskSnapshot;
	RiftRiskSnapshot.Stability = FMath::Clamp(RiftRiskSnapshot.Stability, 0.0f, 100.0f);
	RiftRiskSnapshot.EnemyBudgetMultiplier = FMath::Clamp(RiftRiskSnapshot.EnemyBudgetMultiplier, 0.25f, 3.0f);
	RiftRiskSnapshot.RewardMultiplier = FMath::Clamp(RiftRiskSnapshot.RewardMultiplier, 0.25f, 3.0f);
	RiftRiskSnapshot.PeakRewardMultiplier = FMath::Clamp(RiftRiskSnapshot.PeakRewardMultiplier, 0.25f, 3.0f);
	RiftRiskSnapshot.EnvironmentalPollutionDamage = FMath::Max(0.0f, RiftRiskSnapshot.EnvironmentalPollutionDamage);
	RiftRiskSnapshot.EnvironmentalPulseIntervalSeconds = FMath::Max(0.0f, RiftRiskSnapshot.EnvironmentalPulseIntervalSeconds);
}

void APRRiftGameState::SetMissionModifierSummaries(const TArray<FPRMissionModifierSummary>& InModifierSummaries)
{
	MissionModifierSummaries = InModifierSummaries;
}

void APRRiftGameState::SetMissionRuleSnapshot(const FPRMissionRuleSnapshot& InRuleSnapshot)
{
	MissionRuleSnapshot = InRuleSnapshot;
}

void APRRiftGameState::SetSettlementData(const FPRRiftSettlementData& InSettlementData)
{
	SettlementData = InSettlementData;
	SettlementData.MissionTime = FMath::Max(0.0f, SettlementData.MissionTime);
	SettlementData.RiftStability = FMath::Clamp(SettlementData.RiftStability, 0.0f, 100.0f);
	SettlementData.ObjectiveProgress = FMath::Clamp(SettlementData.ObjectiveProgress, 0.0f, 1.0f);
	SettlementData.AlivePlayerCount = FMath::Max(0, SettlementData.AlivePlayerCount);
	SettlementData.ExtractedPlayerCount = FMath::Max(0, SettlementData.ExtractedPlayerCount);
	SettlementData.KilledEnemyCount = FMath::Max(0, SettlementData.KilledEnemyCount);
	SettlementData.ExtractedItemCount = FMath::Max(0, SettlementData.ExtractedItemCount);
	SettlementData.ExtractedUniqueItemTypes = FMath::Max(0, SettlementData.ExtractedUniqueItemTypes);
	SettlementData.ExtractedResourceCount = FMath::Max(0, SettlementData.ExtractedResourceCount);
	SettlementData.ExtractedUniqueResourceTypes = FMath::Max(0, SettlementData.ExtractedUniqueResourceTypes);
	SettlementData.LostResourceCount = FMath::Max(0, SettlementData.LostResourceCount);
}

void APRRiftGameState::SetSettlementReady(const bool bInSettlementReady)
{
	bSettlementReady = bInSettlementReady;
}

void APRRiftGameState::ResetSettlementData()
{
	SettlementData = FPRRiftSettlementData();
	bSettlementReady = false;
}

void APRRiftGameState::OnRep_CurrentObjectiveState()
{
}

void APRRiftGameState::OnRep_RiftStability()
{
}

void APRRiftGameState::OnRep_ExtractionAvailable()
{
}

void APRRiftGameState::OnRep_ExtractedPlayerCount()
{
}

void APRRiftGameState::OnRep_ExtractionComplete()
{
}

void APRRiftGameState::OnRep_AlivePlayerCount()
{
}

void APRRiftGameState::OnRep_DifficultyPlayerCount()
{
}

void APRRiftGameState::OnRep_MissionTime()
{
}

void APRRiftGameState::OnRep_ObjectiveProgress()
{
}

void APRRiftGameState::OnRep_KilledEnemyCount()
{
}

void APRRiftGameState::OnRep_SettlementData()
{
}

void APRRiftGameState::OnRep_SettlementReady()
{
}
