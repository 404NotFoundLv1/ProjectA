#include "Characters/PRCharacter.h"

#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

APRCharacter::APRCharacter()
{
	bReplicates = true;
	SetReplicatingMovement(true);

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpActionAsset.Succeeded())
	{
		JumpAction = JumpActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	if (MoveActionAsset.Succeeded())
	{
		MoveAction = MoveActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	if (LookActionAsset.Succeeded())
	{
		LookAction = LookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookActionAsset(TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
	if (MouseLookActionAsset.Succeeded())
	{
		MouseLookAction = MouseLookActionAsset.Object;
	}
}
