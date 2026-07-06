#include "Abilities/PRAbilitySystemComponent.h"

UPRAbilitySystemComponent::UPRAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}
