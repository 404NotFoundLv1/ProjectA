#include "Core/PRGameInstance.h"

#include "ProjectA.h"

void UPRGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogProjectA, Log, TEXT("ProjectRift game instance initialized."));
}
