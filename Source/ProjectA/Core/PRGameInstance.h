#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PRGameInstance.generated.h"

/**
 * Project-level runtime owner for menus, sessions, and cross-map state.
 */
UCLASS()
class PROJECTA_API UPRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};
