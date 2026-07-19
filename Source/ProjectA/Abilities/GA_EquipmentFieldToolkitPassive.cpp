#include "Abilities/GA_EquipmentFieldToolkitPassive.h"

UGA_EquipmentFieldToolkitPassive::UGA_EquipmentFieldToolkitPassive()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}
