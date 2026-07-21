#include "Enemies/PREnemyThreatComponent.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

UPREnemyThreatComponent::UPREnemyThreatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPREnemyThreatComponent::AddThreat(APRCharacter* Source, const float Amount)
{
	if (Source && Source->IsAlive() && FMath::IsFinite(Amount) && Amount > 0.0f)
	{
		ThreatByPlayer.FindOrAdd(Source) += Amount;
	}
}

float UPREnemyThreatComponent::GetThreatFor(const APRCharacter* Source) const
{
	return Source ? ThreatByPlayer.FindRef(const_cast<APRCharacter*>(Source)) : 0.0f;
}

void UPREnemyThreatComponent::PruneInvalidThreat()
{
	for (auto It = ThreatByPlayer.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Key()->IsAlive())
		{
			It.RemoveCurrent();
		}
	}
}

APRCharacter* UPREnemyThreatComponent::SelectTarget(const EPREnemyTargetPolicy Policy) const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!Owner || !GameState)
	{
		return nullptr;
	}

	APRCharacter* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
	{
		APRCharacter* Candidate = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
		if (!Candidate || !Candidate->IsAlive())
		{
			continue;
		}
		const float Distance = FVector::DistSquared(Owner->GetActorLocation(), Candidate->GetActorLocation());
		float Score = 0.0f;
		switch (Policy)
		{
		case EPREnemyTargetPolicy::LowestHealth:
		{
			const UPRAttributeSet* Attributes = Candidate->GetProjectRiftAbilitySystemComponent()
				? Candidate->GetProjectRiftAbilitySystemComponent()->GetSet<UPRAttributeSet>() : nullptr;
			Score = Attributes && Attributes->GetMaxHealth() > 0.0f ? 1.0f - Attributes->GetHealth() / Attributes->GetMaxHealth() : 0.0f;
			break;
		}
		case EPREnemyTargetPolicy::Isolated:
			Score = Distance; // isolated policy favors a target farther from this pressure source.
			break;
		case EPREnemyTargetPolicy::AllySupport:
		case EPREnemyTargetPolicy::Nearest:
		default:
			Score = -Distance + GetThreatFor(Candidate) * 10000.0f;
			break;
		}
		if (!Best || Score > BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
	}
	return Best;
}
