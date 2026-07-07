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

	TestNotNull(TEXT("DoInteract input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoInteract")));
	TestNotNull(TEXT("DoPrimaryAttack input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoPrimaryAttack")));
	TestNotNull(TEXT("DoDodge input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoDodge")));
	TestNotNull(TEXT("DoSkillQ input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillQ")));
	TestNotNull(TEXT("DoSkillE input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillE")));
	TestNotNull(TEXT("DoSkillR input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoSkillR")));
	TestNotNull(TEXT("DoOpenInventory input handler exists"), CharacterClass->FindFunctionByName(TEXT("DoOpenInventory")));

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
