#include "Player/PRPlayerController.h"

#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

APRPlayerController::APRPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextAsset(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (DefaultContextAsset.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContextAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContextAsset(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
	if (MouseLookContextAsset.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookContextAsset.Object);
	}
}
