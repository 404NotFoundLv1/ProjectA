#include "Abilities/GA_EngineerDeployableAbility.h"

#include "Deployables/PRDeployableComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Player/PRPlayerState.h"
#include "Roles/PREngineerModuleDataAsset.h"

const UPREngineerModuleDataAsset* UGA_EngineerDeployableAbility::GetEngineerModuleDefinition(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPREngineerModuleDataAsset* Module = Spec ? Cast<UPREngineerModuleDataAsset>(Spec->SourceObject.Get()) : nullptr;
	return Module && Module->GameplayAbilityClass == GetClass() && Module->DeployableKind == GetDeployableKind() ? Module : nullptr;
}

void UGA_EngineerDeployableAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UPREngineerModuleDataAsset* Module = GetEngineerModuleDefinition(Handle, ActorInfo);
	APRPlayerState* PlayerState = ActorInfo ? Cast<APRPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	UPRDeployableComponent* Deployables = PlayerState ? PlayerState->GetDeployableComponent() : nullptr;
	FString Diagnostic;
	if (!Module || !Deployables || !CanActivateAbility(Handle, ActorInfo) || !Deployables->BeginPlacement(Module, Diagnostic))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
