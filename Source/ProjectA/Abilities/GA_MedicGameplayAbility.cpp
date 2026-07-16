#include "Abilities/GA_MedicGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

const UPRMedicModuleDataAsset* UGA_MedicGameplayAbility::GetMedicModuleDefinition(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const EPRMedicModuleKind ExpectedKind) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPRMedicModuleDataAsset* Module = Spec ? Cast<UPRMedicModuleDataAsset>(Spec->SourceObject.Get()) : nullptr;
	return Module && Module->GameplayAbilityClass == GetClass() && Module->MedicKind == ExpectedKind ? Module : nullptr;
}
