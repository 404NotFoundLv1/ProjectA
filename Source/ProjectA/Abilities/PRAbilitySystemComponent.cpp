#include "Abilities/PRAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"

UPRAbilitySystemComponent::UPRAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

bool UPRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	bool bHandledInput = false;

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		bHandledInput = true;
		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputPressed(Spec.Handle);
			}

			AbilitySpecInputPressed(Spec);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
			TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			const FGameplayAbilityActivationInfo& ActivationInfo = Instances.IsEmpty() ? Spec.ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, ActivationInfo.GetActivationPredictionKey());
		}
		else
		{
			TryActivateAbility(Spec.Handle);
		}
	}

	return bHandledInput;
}

bool UPRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	bool bHandledInput = false;

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		bHandledInput = true;
		Spec.InputPressed = false;

		if (Spec.Ability && Spec.IsActive())
		{
			if (Spec.Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
			{
				ServerSetInputReleased(Spec.Handle);
			}

			AbilitySpecInputReleased(Spec);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
			TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			const FGameplayAbilityActivationInfo& ActivationInfo = Instances.IsEmpty() ? Spec.ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, ActivationInfo.GetActivationPredictionKey());
		}
	}

	return bHandledInput;
}

bool UPRAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return TryActivateAbility(Spec.Handle);
		}
	}

	return false;
}
