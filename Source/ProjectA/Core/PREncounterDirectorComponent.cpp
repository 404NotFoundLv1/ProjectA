#include "Core/PREncounterDirectorComponent.h"

#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Core/PRRiftRuleComponent.h"
#include "Core/PRSpawnManager.h"
#include "Diagnostics/PRDiagnosticsLog.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "Settings/PRProjectSettings.h"
#include "TimerManager.h"

UPREncounterDirectorComponent::UPREncounterDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

float UPREncounterDirectorComponent::CalculateTargetThreatBudget(const FPREncounterScalingSnapshot& Scaling) const
{
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float BaseThreat = Settings ? FMath::Max(0.0f, Settings->EncounterBaseThreatBudget) : 4.0f;
	const float PerPlayer = Settings ? FMath::Max(0.0f, Settings->EncounterThreatPerAdditionalPlayer) : 2.0f;
	const float Maximum = Settings ? FMath::Max(1.0f, Settings->EncounterMaximumThreatBudget) : 16.0f;
	const int32 PlayerCount = FMath::Max(1, Scaling.FrozenPlayerCount);
	const float Multiplier = FMath::Max(0.0f, Scaling.RiskEnemyBudgetMultiplier);
	return FMath::Clamp((BaseThreat + (PlayerCount - 1) * PerPlayer) * Multiplier, 0.0f, Maximum);
}

void UPREncounterDirectorComponent::SetEncounterManagers(const TArray<APRSpawnManager*>& InManagers)
{
	EncounterManagers.Reset();
	for (APRSpawnManager* Manager : InManagers)
	{
		if (IsValid(Manager))
		{
			EncounterManagers.Add(Manager);
		}
	}
	EncounterManagers.Sort([](const APRSpawnManager& Left, const APRSpawnManager& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
}

bool UPREncounterDirectorComponent::StartEncounter(APRRiftObjectiveActor* ObjectiveActor, const int32 InRunSeed)
{
	APRRiftGameMode* GameMode = Cast<APRRiftGameMode>(GetOwner());
	UWorld* World = GetWorld();
	if (!GameMode || !World || !GameMode->HasAuthority() || EncounterManagers.IsEmpty())
	{
		return false;
	}

	StopEncounter();
	ActiveObjective = ObjectiveActor;
	RunSeed = InRunSeed;
	DecisionOrdinal = 0;
	for (APRSpawnManager* Manager : EncounterManagers)
	{
		Manager->StartSpawningForObjective(ObjectiveActor);
	}

	const FPREncounterScalingSnapshot Scaling = BuildScalingSnapshot();
	const float Now = World->GetTimeSeconds();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	PressureEndsAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterPressureDuration) : 20.0f);
	NextReinforcementAt = Now;
	Snapshot.Phase = EPREncounterPhase::Pressure;
	TrySpawnReinforcement(Scaling, true);
	UpdateSnapshot(Scaling);
	const float SampleInterval = Settings ? FMath::Max(0.1f, Settings->EncounterSampleInterval) : 1.0f;
	World->GetTimerManager().SetTimer(EncounterTimerHandle, this, &UPREncounterDirectorComponent::TickEncounter, SampleInterval, true);
	return true;
}

void UPREncounterDirectorComponent::StopEncounter()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EncounterTimerHandle);
	}
	for (APRSpawnManager* Manager : EncounterManagers)
	{
		if (IsValid(Manager))
		{
			Manager->StopSpawning();
		}
	}
	ActiveObjective = nullptr;
	Snapshot = FPREncounterDirectorSnapshot();
	if (APRRiftGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr)
	{
		GameState->SetEncounterDirectorSnapshot(Snapshot);
	}
}

void UPREncounterDirectorComponent::TickEncounter()
{
	UWorld* World = GetWorld();
	if (!World || !ActiveObjective || Snapshot.Phase == EPREncounterPhase::Inactive)
	{
		return;
	}

	const FPREncounterScalingSnapshot Scaling = BuildScalingSnapshot();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	const float Now = World->GetTimeSeconds();
	const bool bNeedsRespite = Scaling.DownedPlayerCount > 0 || Scaling.bAllStandingPlayersLowHealth;
	if (bNeedsRespite && Now >= NextRespiteAllowedAt && Snapshot.Phase != EPREncounterPhase::Respite)
	{
		Snapshot.Phase = EPREncounterPhase::Respite;
		RespiteEndsAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterRespiteDuration) : 12.0f);
		NextRespiteAllowedAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterRespiteCooldown) : 45.0f);
		SetDecision(TEXT("Encounter.Respite"));
		UpdateSnapshot(Scaling);
		return;
	}
	if (Snapshot.Phase == EPREncounterPhase::Respite)
	{
		if (Now < RespiteEndsAt)
		{
			UpdateSnapshot(Scaling);
			return;
		}
		Snapshot.Phase = EPREncounterPhase::Pressure;
		PressureEndsAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterPressureDuration) : 20.0f);
		NextReinforcementAt = Now;
	}
	if (Snapshot.Phase == EPREncounterPhase::Telegraph)
	{
		if (Now < NextReinforcementAt)
		{
			UpdateSnapshot(Scaling);
			return;
		}
		Snapshot.Phase = EPREncounterPhase::Pressure;
		TrySpawnReinforcement(Scaling, false);
		UpdateSnapshot(Scaling);
		return;
	}
	if (Snapshot.Phase == EPREncounterPhase::Pressure && Now >= PressureEndsAt)
	{
		Snapshot.Phase = EPREncounterPhase::Cooldown;
		CooldownEndsAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterCooldownDuration) : 8.0f);
		SetDecision(TEXT("Encounter.Cooldown"));
	}
	if (Snapshot.Phase == EPREncounterPhase::Cooldown)
	{
		if (Now < CooldownEndsAt)
		{
			UpdateSnapshot(Scaling);
			return;
		}
		Snapshot.Phase = EPREncounterPhase::Pressure;
		PressureEndsAt = Now + (Settings ? FMath::Max(0.1f, Settings->EncounterPressureDuration) : 20.0f);
		NextReinforcementAt = Now;
	}
	if (Now >= NextReinforcementAt)
	{
		Snapshot.Phase = EPREncounterPhase::Telegraph;
		NextReinforcementAt = Now + (Settings ? FMath::Max(0.0f, Settings->EncounterTelegraphDuration) : 1.5f);
		SetDecision(TEXT("Encounter.Telegraph"));
	}
	UpdateSnapshot(Scaling);
}

FPREncounterScalingSnapshot UPREncounterDirectorComponent::BuildScalingSnapshot() const
{
	FPREncounterScalingSnapshot Scaling;
	const APRRiftGameMode* GameMode = Cast<APRRiftGameMode>(GetOwner());
	const APRRiftGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr;
	Scaling.FrozenPlayerCount = GameState ? FMath::Max(1, GameState->GetDifficultyPlayerCount()) : 1;
	Scaling.ObjectiveNodeId = ActiveObjective ? ActiveObjective->GetObjectiveNodeId() : NAME_None;
	Scaling.EncounterRegionId = EncounterManagers.IsEmpty() ? NAME_None : EncounterManagers[0]->GetEncounterRegionId();
	if (GameState)
	{
		Scaling.RiftStability = GameState->GetRiftStability();
		Scaling.RiskEnemyBudgetMultiplier = FMath::Max(0.0f, GameState->GetRiftRiskSnapshot().EnemyBudgetMultiplier);
		float StandingHealthTotal = 0.0f;
		int32 StandingPlayerCount = 0;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!PlayerState || PlayerState->IsOnlyASpectator()) continue;
			APRCharacter* Character = nullptr;
			if (const AController* Controller = Cast<AController>(PlayerState->GetOwner())) Character = Cast<APRCharacter>(Controller->GetPawn());
			if (!Character) continue;
			if (Character->IsDowned()) { ++Scaling.DownedPlayerCount; continue; }
			if (!Character->IsAlive()) continue;
			++Scaling.ActivePlayerCount;
			++StandingPlayerCount;
			if (const UPRAttributeSet* Attributes = Character->GetAttributeSet())
			{
				StandingHealthTotal += FMath::Clamp(Attributes->GetHealth() / FMath::Max(1.0f, Attributes->GetMaxHealth()), 0.0f, 1.0f);
			}
			else { StandingHealthTotal += 1.0f; }
		}
		Scaling.TeamHealthFraction = StandingPlayerCount > 0 ? StandingHealthTotal / StandingPlayerCount : 0.0f;
		const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
		Scaling.bAllStandingPlayersLowHealth = StandingPlayerCount > 0 && Scaling.TeamHealthFraction <= (Settings ? Settings->EncounterLowHealthThreshold : 0.35f);
	}
	return Scaling;
}

void UPREncounterDirectorComponent::UpdateSnapshot(const FPREncounterScalingSnapshot& Scaling)
{
	Snapshot.TargetThreatBudget = CalculateTargetThreatBudget(Scaling);
	if (Snapshot.Phase == EPREncounterPhase::Respite)
	{
		const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
		Snapshot.TargetThreatBudget = FMath::Max(Snapshot.TargetThreatBudget * (Settings ? Settings->EncounterPostRespiteBudgetFloor : 0.5f), 0.0f);
	}
	Snapshot.AliveThreat = GetAliveThreat();
	Snapshot.AliveEnemyCount = GetAliveEnemyCount();
	Snapshot.ObjectiveNodeId = Scaling.ObjectiveNodeId;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Next = Snapshot.Phase == EPREncounterPhase::Respite ? RespiteEndsAt : Snapshot.Phase == EPREncounterPhase::Cooldown ? CooldownEndsAt : NextReinforcementAt;
	Snapshot.NextEventRemainingSeconds = FMath::Max(0.0f, Next - Now);
	if (APRRiftGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRRiftGameState>() : nullptr) GameState->SetEncounterDirectorSnapshot(Snapshot);
}

bool UPREncounterDirectorComponent::TrySpawnReinforcement(const FPREncounterScalingSnapshot& Scaling, const bool bImmediate)
{
	UWorld* World = GetWorld();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	if (!World) return false;
	const float TargetThreat = CalculateTargetThreatBudget(Scaling);
	APRSpawnManager* Manager = SelectManager(Scaling);
	if (!Manager) { SetDecision(TEXT("Encounter.NoRegion"), TEXT("No active spawn manager.")); return false; }
	const int32 HardCap = Manager->GetMaxAliveEnemiesForScaling();
	if (GetAliveEnemyCount() >= HardCap || GetAliveThreat() >= TargetThreat)
	{
		SetDecision(TEXT("Encounter.AtCap"));
		NextReinforcementAt = World->GetTimeSeconds() + (Settings ? Settings->EncounterReinforcementInterval : 6.0f);
		return false;
	}
	FPREncounterSpawnRequest Request = BuildBasicRequest(Scaling, Manager);
	if (Manager->ExecuteEncounterSpawnRequest(Request) <= 0)
	{
		SetDecision(TEXT("Encounter.NoSafeSpawn"), TEXT("No configured spawn point passed the safe-spawn checks."));
		NextReinforcementAt = World->GetTimeSeconds() + (Settings ? Settings->EncounterUnsafeRetryDelay : 2.0f);
		return false;
	}
	++DecisionOrdinal;
	SetDecision(bImmediate ? TEXT("Encounter.InitialSpawn") : TEXT("Encounter.Reinforcement"));
	NextReinforcementAt = World->GetTimeSeconds() + (Settings ? Settings->EncounterReinforcementInterval : 6.0f);
	return true;
}

void UPREncounterDirectorComponent::SetDecision(const FName DecisionCode, const FString& RejectionReason)
{
	Snapshot.LastDecisionCode = DecisionCode;
	Snapshot.LastRejectionReason = RejectionReason;
	FPRDiagnosticContext Context;
	Context.RunId = Cast<APRRiftGameMode>(GetOwner()) ? Cast<APRRiftGameMode>(GetOwner())->GetCurrentRunId() : FGuid();
	PRRecordDiagnosticEvent(this, RejectionReason.IsEmpty() ? EPRDiagnosticSeverity::Info : EPRDiagnosticSeverity::Warning, TEXT("Encounter"), DecisionCode,
		FString::Printf(TEXT("Seed=%d Ordinal=%d Objective=%s %s"), RunSeed, DecisionOrdinal, *Snapshot.ObjectiveNodeId.ToString(), *RejectionReason), Context);
}

APRSpawnManager* UPREncounterDirectorComponent::SelectManager(const FPREncounterScalingSnapshot& Scaling) const
{
	for (APRSpawnManager* Manager : EncounterManagers)
	{
		if (IsValid(Manager) && (Scaling.EncounterRegionId.IsNone() || Manager->GetEncounterRegionId() == Scaling.EncounterRegionId)) return Manager;
	}
	return nullptr;
}

FPREncounterSpawnRequest UPREncounterDirectorComponent::BuildBasicRequest(const FPREncounterScalingSnapshot& Scaling, APRSpawnManager* Manager) const
{
	FPREncounterSpawnRequest Request;
	Request.EntryId = TEXT("Enemy.Basic");
	Request.Category = EPREncounterUnitCategory::Melee;
	Request.ThreatCost = 1.0f;
	Request.ObjectiveNodeId = Scaling.ObjectiveNodeId;
	Request.DecisionOrdinal = DecisionOrdinal;
	Request.RunSeed = RunSeed;
	const TArray<FPREncounterSpawnEntry>& Entries = Manager->GetEncounterSpawnEntries();
	if (!Entries.IsEmpty())
	{
		TArray<const FPREncounterSpawnEntry*> SortedEntries;
		const float RemainingThreat = CalculateTargetThreatBudget(Scaling) - GetAliveThreat();
		for (const FPREncounterSpawnEntry& Entry : Entries)
		{
			if (Entry.EnemyClass && Entry.ThreatCost > 0.0f && Entry.ThreatCost <= RemainingThreat && Entry.SelectionWeight > 0.0f && CanSpawnCategory(Entry.Category, Scaling)) SortedEntries.Add(&Entry);
		}
		SortedEntries.Sort([](const FPREncounterSpawnEntry& Left, const FPREncounterSpawnEntry& Right) { return Left.EntryId.LexicalLess(Right.EntryId); });
		if (!SortedEntries.IsEmpty())
		{
			FRandomStream Random(GetDeterministicSeed(Scaling));
			float TotalWeight = 0.0f;
			for (const FPREncounterSpawnEntry* Entry : SortedEntries) TotalWeight += Entry->SelectionWeight;
			float Pick = Random.FRandRange(0.0f, TotalWeight);
			const FPREncounterSpawnEntry* Selected = SortedEntries.Last();
			for (const FPREncounterSpawnEntry* Entry : SortedEntries) { Pick -= Entry->SelectionWeight; if (Pick <= 0.0f) { Selected = Entry; break; } }
			const FPREncounterSpawnEntry& Entry = *Selected;
			Request.EntryId = Entry.EntryId; Request.EnemyClass = Entry.EnemyClass; Request.Category = Entry.Category; Request.ThreatCost = Entry.ThreatCost;
		}
	}
	const APRRiftGameMode* GameMode = Cast<APRRiftGameMode>(GetOwner());
	if (GameMode && GameMode->GetRiftRuleComponent() && GameMode->GetRiftRuleComponent()->HasModifier(TEXT("Modifier.EliteReinforcements")) && CanSpawnCategory(EPREncounterUnitCategory::Elite, Scaling) && CalculateTargetThreatBudget(Scaling) - GetAliveThreat() >= 3.0f)
	{
		Request.bElite = true; Request.Category = EPREncounterUnitCategory::Elite; Request.ThreatCost = 3.0f;
	}
	return Request;
}

bool UPREncounterDirectorComponent::CanSpawnCategory(const EPREncounterUnitCategory Category, const FPREncounterScalingSnapshot& Scaling) const
{
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();
	switch (Category)
	{
	case EPREncounterUnitCategory::Elite:
		return GetAliveCategoryCount(Category) < (Scaling.FrozenPlayerCount >= 4 ? (Settings ? Settings->EncounterLargePartyEliteCap : 2) : (Settings ? Settings->EncounterSoloEliteCap : 1));
	case EPREncounterUnitCategory::Ranged:
		return GetAliveCategoryCount(Category) < FMath::DivideAndRoundUp(FMath::Max(1, Scaling.FrozenPlayerCount), 2);
	case EPREncounterUnitCategory::Exploder:
		return GetAliveCategoryCount(Category) < (Settings ? Settings->EncounterExploderCap : 1);
	default:
		return true;
	}
}

int32 UPREncounterDirectorComponent::GetAliveCategoryCount(const EPREncounterUnitCategory Category) const
{
	int32 Total = 0;
	for (const APRSpawnManager* Manager : EncounterManagers) if (IsValid(Manager)) Total += Manager->GetAliveEncounterCategoryCount(Category);
	return Total;
}

float UPREncounterDirectorComponent::GetAliveThreat() const { float Total = 0.0f; for (const APRSpawnManager* Manager : EncounterManagers) if (IsValid(Manager)) Total += Manager->GetAliveEncounterThreat(); return Total; }
int32 UPREncounterDirectorComponent::GetAliveEnemyCount() const { int32 Total = 0; for (const APRSpawnManager* Manager : EncounterManagers) if (IsValid(Manager)) Total += Manager->GetAliveEnemyCount(); return Total; }
int32 UPREncounterDirectorComponent::GetDeterministicSeed(const FPREncounterScalingSnapshot& Scaling) const { return HashCombineFast(HashCombineFast(::GetTypeHash(RunSeed), Scaling.ObjectiveNodeId.GetComparisonIndex().ToUnstableInt()), ::GetTypeHash(DecisionOrdinal)); }
