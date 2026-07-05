#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/PRCharacter.h"
#include "InputAction.h"
#include "UObject/UnrealType.h"

namespace
{
bool HasInputActionProperty(UClass* Class, const FName PropertyName)
{
	const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Class, PropertyName);
	return Property && Property->PropertyClass && Property->PropertyClass->IsChildOf(UInputAction::StaticClass());
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

	return true;
}

#endif
