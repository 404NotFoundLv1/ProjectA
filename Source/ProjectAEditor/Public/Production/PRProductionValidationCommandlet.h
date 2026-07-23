#pragma once

#include "Commandlets/Commandlet.h"
#include "PRProductionValidationCommandlet.generated.h"

UCLASS()
class PROJECTAEDITOR_API UPRProductionValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPRProductionValidationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
