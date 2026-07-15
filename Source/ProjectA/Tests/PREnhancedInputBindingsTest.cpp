#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/PRCharacter.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRPickupActor.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace
{
bool HasInputActionProperty(UClass* Class, const FName PropertyName)
{
	const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Class, PropertyName);
	return Property && Property->PropertyClass && Property->PropertyClass->IsChildOf(UInputAction::StaticClass());
}

bool HasLegacyPressedKeyBinding(const UInputComponent* InputComponent, const FKey Key)
{
	if (!InputComponent)
	{
		return false;
	}

	for (const FInputKeyBinding& KeyBinding : InputComponent->KeyBindings)
	{
		if (KeyBinding.Chord.Key == Key && KeyBinding.KeyEvent == IE_Pressed)
		{
			return true;
		}
	}

	return false;
}

bool MappingContextHasKey(const UInputMappingContext* Context, const UInputAction* Action, const FKey Key)
{
	if (!Context || !Action)
	{
		return false;
	}

	return Context->GetMappings().ContainsByPredicate(
		[Action, Key](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Action && Mapping.Key == Key;
		});
}

FPRItemInstance MakeEnhancedInputTestItem(const FName ItemId, const int32 Count = 1)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPREnhancedInputBindingsTest, "ProjectRift.Input.EnhancedBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREnhancedInputBindingsTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = APRCharacter::StaticClass();

	TestTrue(TEXT("InteractAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("InteractAction")));
	TestTrue(TEXT("PrimaryAttackAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("PrimaryAttackAction")));
	TestTrue(TEXT("DodgeAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("DodgeAction")));
	TestTrue(TEXT("SkillQAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("SkillQAction")));
	TestTrue(TEXT("SkillEAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("SkillEAction")));
	TestTrue(TEXT("SkillRAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("SkillRAction")));
	TestTrue(TEXT("OpenInventoryAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("OpenInventoryAction")));
	TestTrue(TEXT("AimAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("AimAction")));
	TestTrue(TEXT("ReloadAction exists as an input action property"), HasInputActionProperty(CharacterClass, TEXT("ReloadAction")));

	TestNotNull(TEXT("DoInteract input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoInteract")));
	TestNotNull(TEXT("DoPrimaryAttack input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoPrimaryAttack")));
	TestNotNull(TEXT("DoDodge input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoDodge")));
	TestNotNull(TEXT("DoSkillQ input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillQ")));
	TestNotNull(TEXT("DoSkillE input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillE")));
	TestNotNull(TEXT("DoSkillR input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillR")));
	TestNotNull(TEXT("DoOpenInventory input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoOpenInventory")));
	TestNotNull(TEXT("DoAim input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoAim")));
	TestNotNull(TEXT("DoAimReleased input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoAimReleased")));
	TestNotNull(TEXT("DoReload input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoReload")));

	const APRCharacter* CharacterCDO = CharacterClass->GetDefaultObject<APRCharacter>();
	const FObjectPropertyBase* AimProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("AimAction"));
	const FObjectPropertyBase* ReloadProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("ReloadAction"));
	const UInputAction* AimAction = AimProperty ? Cast<UInputAction>(AimProperty->GetObjectPropertyValue_InContainer(CharacterCDO)) : nullptr;
	const UInputAction* ReloadAction = ReloadProperty ? Cast<UInputAction>(ReloadProperty->GetObjectPropertyValue_InContainer(CharacterCDO)) : nullptr;
	const FObjectPropertyBase* PrimaryAttackProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("PrimaryAttackAction"));
	const UInputAction* PrimaryAttackAction = PrimaryAttackProperty ? Cast<UInputAction>(PrimaryAttackProperty->GetObjectPropertyValue_InContainer(CharacterCDO)) : nullptr;
	TestNotNull(TEXT("Character loads IA_Aim"), AimAction);
	TestNotNull(TEXT("Character loads IA_Reload"), ReloadAction);
	const UInputMappingContext* DefaultContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ProjectRift/Input/IMC_Default.IMC_Default"));
	TestNotNull(TEXT("Default input context loads"), DefaultContext);
	TestTrue(TEXT("Right mouse holds aim"), MappingContextHasKey(DefaultContext, AimAction, EKeys::RightMouseButton));
	TestTrue(TEXT("Left trigger holds aim"), MappingContextHasKey(DefaultContext, AimAction, EKeys::Gamepad_LeftTrigger));
	TestTrue(TEXT("R starts reload"), MappingContextHasKey(DefaultContext, ReloadAction, EKeys::R));
	TestTrue(TEXT("Gamepad X starts reload"), MappingContextHasKey(DefaultContext, ReloadAction, EKeys::Gamepad_FaceButton_Left));
	TestTrue(TEXT("Right trigger keeps primary attack"), MappingContextHasKey(DefaultContext, PrimaryAttackAction, EKeys::Gamepad_RightTrigger));
	const UInputAction* SkillRAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_R.IA_Skill_R"));
	TestFalse(TEXT("R is no longer double-bound to Skill R"), MappingContextHasKey(DefaultContext, SkillRAction, EKeys::R));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Enhanced input test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Enhanced input test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRPlayerController* PlayerController = World->SpawnActor<APRPlayerController>();
	APRPlayerState* PlayerState = World->SpawnActor<APRPlayerState>();
	APRCharacter* Character = World->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
	APRPickupActor* Pickup = World->SpawnActor<APRPickupActor>(FVector(80.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Can spawn input test player controller"), PlayerController);
	TestNotNull(TEXT("Can spawn input test player state"), PlayerState);
	TestNotNull(TEXT("Can spawn input test character"), Character);
	TestNotNull(TEXT("Can spawn input test pickup"), Pickup);
	if (!PlayerController || !PlayerState || !Character || !Pickup)
	{
		return false;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->SetPlayer(NewObject<ULocalPlayer>(GEngine));
	PlayerController->InitInputSystem();
	TestNotNull(TEXT("Player controller input component is initialized"), PlayerController->InputComponent.Get());
	TestFalse(TEXT("Tab is handled only by IA_OpenInventory, not a legacy PlayerController key binding"), HasLegacyPressedKeyBinding(PlayerController->InputComponent, EKeys::Tab));
	TestFalse(TEXT("F is handled only by IA_Interact, not a legacy PlayerController key binding"), HasLegacyPressedKeyBinding(PlayerController->InputComponent, EKeys::F));
	TestFalse(TEXT("R is handled only by IA_Reload, not a legacy PlayerController key binding"), HasLegacyPressedKeyBinding(PlayerController->InputComponent, EKeys::R));

	PlayerController->Possess(Character);
	Pickup->SetItemInstance(MakeEnhancedInputTestItem(TEXT("EnergyCrystal"), 1));

	UPRInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	TestNotNull(TEXT("Input test player owns inventory"), Inventory);
	if (!Inventory)
	{
		return false;
	}

	Character->DoInteract();
	TestEqual(TEXT("IA_Interact handler picks up nearby item through player controller"), Inventory->GetItemCount(TEXT("EnergyCrystal")), 1);

	Character->DoOpenInventory();
	TestTrue(TEXT("IA_OpenInventory handler toggles the player inventory UI"), PlayerController->IsInventoryVisible());

	return true;
}

#endif
