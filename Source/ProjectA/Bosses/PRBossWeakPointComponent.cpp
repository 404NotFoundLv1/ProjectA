#include "Bosses/PRBossWeakPointComponent.h"

UPRBossWeakPointComponent::UPRBossWeakPointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPRBossWeakPointComponent::Configure(const FPRBossWeakPointDefinition& InDefinition)
{
	WeakPointId = InDefinition.WeakPointId;
	DamageMultiplier = FMath::IsFinite(InDefinition.DamageMultiplier)
		? FMath::Max(1.0f, InDefinition.DamageMultiplier)
		: 1.0f;
}
