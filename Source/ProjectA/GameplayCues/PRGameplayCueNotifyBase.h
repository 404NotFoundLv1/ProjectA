#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "PRGameplayCueNotifyBase.generated.h"

/** Native base for ProjectRift GameplayCue notifies. */
UCLASS(Abstract, Blueprintable)
class PROJECTA_API UPRGameplayCueNotifyBase : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
