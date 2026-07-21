#include "Core/PRObjectiveTypeActors.h"

#include "Core/PRObjectiveGraphComponent.h"
#include "Core/PRRiftGameMode.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Engine/World.h"

namespace
{
bool AdvanceGraphObjective(APRRiftObjectiveActor* Objective, const int32 CurrentCount)
{
	if (!Objective || !Objective->HasAuthority() || !Objective->IsObjectiveActive() || !Objective->UsesObjectiveGraph())
	{
		return false;
	}
	APRRiftGameMode* GameMode = Objective->GetWorld() ? Objective->GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr;
	return GameMode && GameMode->ReportObjectiveNodeCount(Objective->GetObjectiveNodeId(), CurrentCount);
}

const FPRObjectiveNodeDefinition* GetObjectiveDefinition(const APRRiftObjectiveActor* Objective)
{
	const APRRiftGameMode* GameMode = Objective && Objective->GetWorld() ? Objective->GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr;
	const UPRObjectiveGraphComponent* Graph = GameMode ? GameMode->GetObjectiveGraphComponent() : nullptr;
	return Graph && Objective ? Graph->FindNodeDefinition(Objective->GetObjectiveNodeId()) : nullptr;
}
}

bool APRCollectObjectiveActor::RegisterCollectedItem(const FGuid ItemInstanceGuid)
{
	if (!ItemInstanceGuid.IsValid() || CollectedItemGuids.Contains(ItemInstanceGuid))
	{
		return false;
	}
	CollectedItemGuids.Add(ItemInstanceGuid);
	if (!AdvanceGraphObjective(this, CollectedItemGuids.Num()))
	{
		CollectedItemGuids.Remove(ItemInstanceGuid);
		return false;
	}
	return true;
}

bool APRCarryObjectiveActor::SetTrackedCoreGuid(const FGuid InCoreGuid)
{
	if (!HasAuthority() || !InCoreGuid.IsValid() || TrackedCoreGuid.IsValid())
	{
		return false;
	}
	TrackedCoreGuid = InCoreGuid;
	return true;
}

bool APRCarryObjectiveActor::SubmitTrackedCore(const FGuid CoreGuid)
{
	if (!CoreGuid.IsValid() || CoreGuid != TrackedCoreGuid)
	{
		return false;
	}
	return AdvanceGraphObjective(this, 1);
}

bool APRDestroyObjectiveActor::RegisterDestroyedTarget()
{
	return AdvanceGraphObjective(this, 1);
}

APRDestroyObjectiveActor::APRDestroyObjectiveActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<UPRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UPRAttributeSet>(TEXT("AttributeSet"));
}

void APRDestroyObjectiveActor::BeginPlay()
{
	Super::BeginPlay();
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		const float SafeHealth = FMath::Max(1.0f, TargetHealth);
		AttributeSet->SetMaxHealth(SafeHealth);
		AttributeSet->SetHealth(SafeHealth);
		AttributeSet->SetMaxShield(0.0f);
		AttributeSet->SetShield(0.0f);
		AttributeSet->SetMaxEnergy(0.0f);
		AttributeSet->SetEnergy(0.0f);
		AttributeSet->SetAttackPower(0.0f);
		AttributeSet->SetMoveSpeed(0.0f);
		AttributeSet->SetCooldownReduction(0.0f);
		AttributeSet->SetHealingPower(0.0f);
		AttributeSet->SetPollutionResistance(0.0f);
	}
}

UAbilitySystemComponent* APRDestroyObjectiveActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

bool APRDestroyObjectiveActor::IsCombatUnitInactive() const
{
	return bTargetDestroyed || IsObjectiveCompleted();
}

void APRDestroyObjectiveActor::HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext)
{
	if (!HasAuthority() || bTargetDestroyed)
	{
		return;
	}
	bTargetDestroyed = true;
	if (RegisterDestroyedTarget())
	{
		CompleteObjective();
	}
}

bool APRHuntObjectiveActor::RegisterHuntTargetEliminated(const FName TargetId)
{
	const FPRObjectiveNodeDefinition* Definition = GetObjectiveDefinition(this);
	if (!Definition || Definition->ObjectiveType != EPRObjectiveType::Hunt || Definition->TargetId != TargetId)
	{
		return false;
	}
	const APRRiftGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr;
	const UPRObjectiveGraphComponent* Graph = GameMode ? GameMode->GetObjectiveGraphComponent() : nullptr;
	return AdvanceGraphObjective(this, Graph ? Graph->GetNodeCurrentCount(GetObjectiveNodeId()) + 1 : 1);
}
