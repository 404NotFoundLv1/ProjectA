#include "GameplayCues/PRGameplayCueNotifyBase.h"

#include "Combat/PRCombatFeedbackComponent.h"
#include "GameFramework/Actor.h"

namespace
{
bool RouteGameplayCue(
	AActor* Target,
	const FGameplayTag CueTag,
	const EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (!Target)
	{
		return false;
	}

	UPRCombatFeedbackComponent* Feedback = Target->FindComponentByClass<UPRCombatFeedbackComponent>();
	if (!Feedback)
	{
		return false;
	}

	Feedback->HandleGameplayCue(CueTag, EventType, Parameters);
	return true;
}

FGameplayTag ResolveCueTag(
	const FGameplayCueParameters& Parameters,
	const FGameplayTag FallbackTag)
{
	return Parameters.MatchedTagName.IsValid()
		? Parameters.MatchedTagName
		: FallbackTag;
}
}

bool UPRGameplayCueNotifyBase::OnExecute_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const FGameplayTag CueTag = ResolveCueTag(Parameters, GameplayCueTag);
	return CueTag.IsValid()
		&& RouteGameplayCue(MyTarget, CueTag, EGameplayCueEvent::Executed, Parameters);
}

bool UPRGameplayCueNotifyBase::OnActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const FGameplayTag CueTag = ResolveCueTag(Parameters, GameplayCueTag);
	return CueTag.IsValid()
		&& RouteGameplayCue(MyTarget, CueTag, EGameplayCueEvent::OnActive, Parameters);
}

bool UPRGameplayCueNotifyBase::WhileActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const FGameplayTag CueTag = ResolveCueTag(Parameters, GameplayCueTag);
	return CueTag.IsValid()
		&& RouteGameplayCue(MyTarget, CueTag, EGameplayCueEvent::WhileActive, Parameters);
}

bool UPRGameplayCueNotifyBase::OnRemove_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters) const
{
	const FGameplayTag CueTag = ResolveCueTag(Parameters, GameplayCueTag);
	return CueTag.IsValid()
		&& RouteGameplayCue(MyTarget, CueTag, EGameplayCueEvent::Removed, Parameters);
}
