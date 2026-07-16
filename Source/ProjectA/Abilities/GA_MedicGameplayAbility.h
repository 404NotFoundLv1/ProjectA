#pragma once

#include "CoreMinimal.h"
#include "Abilities/PRRoleModuleGameplayAbility.h"
#include "Roles/PRMedicModuleDataAsset.h"
#include "GA_MedicGameplayAbility.generated.h"

/** Shared source-data validation for the three Medic modules. */
UCLASS(Abstract)
class PROJECTA_API UGA_MedicGameplayAbility : public UPRRoleModuleGameplayAbility
{
	GENERATED_BODY()

protected:
	const UPRMedicModuleDataAsset* GetMedicModuleDefinition(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, EPRMedicModuleKind ExpectedKind) const;
};
