#include "Abilities/PRHitStaggerGameplayEffect.h"

#include "Core/PRGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UPRHitStaggerGameplayEffect::UPRHitStaggerGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this,
			TEXT("HitStaggerTargetTags"));
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(ProjectRiftGameplayTags::State_HitStaggered);
	TargetTagsComponent->SetAndApplyTargetTagChanges(GrantedTags);
	GEComponents.Add(TargetTagsComponent);
}
