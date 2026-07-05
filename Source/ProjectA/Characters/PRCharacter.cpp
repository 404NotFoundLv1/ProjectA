#include "Characters/PRCharacter.h"

#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "ProjectA.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void ShowInputDebugMessage(const UObject* Context, const TCHAR* ActionName)
{
	UE_LOG(LogProjectA, Log, TEXT("Input action triggered: %s"), ActionName);

	if (GEngine && Context)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Cyan,
			FString::Printf(TEXT("Input: %s"), ActionName));
	}
}

FColor GetDebugColorForPlayerId(const int32 PlayerId)
{
	static const FColor Colors[] = {
		FColor::Cyan,
		FColor::Yellow,
		FColor::Green,
		FColor::Orange,
		FColor::Magenta,
		FColor::Red,
	};

	if (PlayerId < 0)
	{
		return FColor::White;
	}

	return Colors[PlayerId % UE_ARRAY_COUNT(Colors)];
}

const TCHAR* NetRoleToText(const ENetRole Role)
{
	switch (Role)
	{
	case ROLE_Authority:
		return TEXT("Authority");
	case ROLE_AutonomousProxy:
		return TEXT("Autonomous");
	case ROLE_SimulatedProxy:
		return TEXT("Simulated");
	default:
		return TEXT("None");
	}
}
}

APRCharacter::APRCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);

	PlayerDebugLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlayerDebugLabel"));
	PlayerDebugLabel->SetupAttachment(GetRootComponent());
	PlayerDebugLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	PlayerDebugLabel->SetHorizontalAlignment(EHTA_Center);
	PlayerDebugLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PlayerDebugLabel->SetTextRenderColor(FColor::White);
	PlayerDebugLabel->SetWorldSize(24.0f);
	PlayerDebugLabel->SetText(FText::FromString(TEXT("Player")));

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpActionAsset.Succeeded())
	{
		JumpAction = JumpActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Move.IA_Move"));
	if (MoveActionAsset.Succeeded())
	{
		MoveAction = MoveActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Look.IA_Look"));
	if (LookActionAsset.Succeeded())
	{
		LookAction = LookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_MouseLook.IA_MouseLook"));
	if (MouseLookActionAsset.Succeeded())
	{
		MouseLookAction = MouseLookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InteractActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Interact.IA_Interact"));
	if (InteractActionAsset.Succeeded())
	{
		InteractAction = InteractActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> PrimaryAttackActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack"));
	if (PrimaryAttackActionAsset.Succeeded())
	{
		PrimaryAttackAction = PrimaryAttackActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Dodge.IA_Dodge"));
	if (DodgeActionAsset.Succeeded())
	{
		DodgeAction = DodgeActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillQActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_Q.IA_Skill_Q"));
	if (SkillQActionAsset.Succeeded())
	{
		SkillQAction = SkillQActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillEActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_E.IA_Skill_E"));
	if (SkillEActionAsset.Succeeded())
	{
		SkillEAction = SkillEActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillRActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_R.IA_Skill_R"));
	if (SkillRActionAsset.Succeeded())
	{
		SkillRAction = SkillRActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> OpenInventoryActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_OpenInventory.IA_OpenInventory"));
	if (OpenInventoryActionAsset.Succeeded())
	{
		OpenInventoryAction = OpenInventoryActionAsset.Object;
	}
}

void APRCharacter::BeginPlay()
{
	Super::BeginPlay();

	RefreshPlayerDebugLabel();
}

void APRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	RefreshPlayerDebugLabel();
}

void APRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	RefreshPlayerDebugLabel();
}

void APRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APRCharacter::DoInteract);
	}

	if (PrimaryAttackAction)
	{
		EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &APRCharacter::DoPrimaryAttack);
	}

	if (DodgeAction)
	{
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &APRCharacter::DoDodge);
	}

	if (SkillQAction)
	{
		EnhancedInputComponent->BindAction(SkillQAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillQ);
	}

	if (SkillEAction)
	{
		EnhancedInputComponent->BindAction(SkillEAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillE);
	}

	if (SkillRAction)
	{
		EnhancedInputComponent->BindAction(SkillRAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillR);
	}

	if (OpenInventoryAction)
	{
		EnhancedInputComponent->BindAction(OpenInventoryAction, ETriggerEvent::Started, this, &APRCharacter::DoOpenInventory);
	}
}

void APRCharacter::RefreshPlayerDebugLabel()
{
	if (!PlayerDebugLabel)
	{
		return;
	}

	const APlayerState* CurrentPlayerState = GetPlayerState();
	const int32 PlayerId = CurrentPlayerState ? CurrentPlayerState->GetPlayerId() : INDEX_NONE;
	const FString PlayerName = CurrentPlayerState ? CurrentPlayerState->GetPlayerName() : TEXT("Unassigned");
	const FString LabelText = FString::Printf(
		TEXT("%s\nP%d %s"),
		*PlayerName,
		PlayerId,
		NetRoleToText(GetLocalRole()));

	PlayerDebugLabel->SetText(FText::FromString(LabelText));
	PlayerDebugLabel->SetTextRenderColor(GetDebugColorForPlayerId(PlayerId));
}

void APRCharacter::DoInteract()
{
	ShowInputDebugMessage(this, TEXT("Interact"));
}

void APRCharacter::DoPrimaryAttack()
{
	ShowInputDebugMessage(this, TEXT("PrimaryAttack"));
}

void APRCharacter::DoDodge()
{
	ShowInputDebugMessage(this, TEXT("Dodge"));
}

void APRCharacter::DoSkillQ()
{
	ShowInputDebugMessage(this, TEXT("Skill Q"));
}

void APRCharacter::DoSkillE()
{
	ShowInputDebugMessage(this, TEXT("Skill E"));
}

void APRCharacter::DoSkillR()
{
	ShowInputDebugMessage(this, TEXT("Skill R"));
}

void APRCharacter::DoOpenInventory()
{
	ShowInputDebugMessage(this, TEXT("OpenInventory"));
}
