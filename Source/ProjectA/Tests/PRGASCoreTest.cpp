#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Player/PRPlayerState.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASCoreTest, "ProjectRift.GAS.Core", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindProjectRiftClass(const TCHAR* ClassPath)
{
	return FindObject<UClass>(nullptr, ClassPath);
}

bool TestGameplayAttributeProperty(
	FAutomationTestBase& Test,
	UClass* AttributeSetClass,
	const TCHAR* PropertyName,
	const TCHAR* RepNotifyFunctionName,
	const float ExpectedDefaultValue)
{
	if (!AttributeSetClass)
	{
		Test.AddError(TEXT("AttributeSet class is missing"));
		return false;
	}

	const FStructProperty* AttributeProperty = FindFProperty<FStructProperty>(AttributeSetClass, PropertyName);
	Test.TestNotNull(FString::Printf(TEXT("%s attribute exists"), PropertyName), AttributeProperty);
	Test.TestTrue(
		FString::Printf(TEXT("%s uses FGameplayAttributeData"), PropertyName),
		AttributeProperty && AttributeProperty->Struct == FGameplayAttributeData::StaticStruct());
	Test.TestTrue(
		FString::Printf(TEXT("%s is replicated"), PropertyName),
		AttributeProperty && AttributeProperty->HasAnyPropertyFlags(CPF_Net));
	Test.TestEqual(
		FString::Printf(TEXT("%s uses GAS RepNotify"), PropertyName),
		AttributeProperty ? AttributeProperty->RepNotifyFunc : NAME_None,
		FName(RepNotifyFunctionName));
	Test.TestNotNull(
		FString::Printf(TEXT("%s RepNotify function exists"), PropertyName),
		AttributeSetClass->FindFunctionByName(FName(RepNotifyFunctionName)));

	const UObject* AttributeDefaults = AttributeSetClass->GetDefaultObject();
	const FGameplayAttributeData* AttributeData = AttributeProperty && AttributeDefaults
		? AttributeProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeDefaults)
		: nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s has default data"), PropertyName), AttributeData);
	if (AttributeData)
	{
		Test.TestEqual(
			FString::Printf(TEXT("%s default current value"), PropertyName),
			AttributeData->GetCurrentValue(),
			ExpectedDefaultValue);
		Test.TestEqual(
			FString::Printf(TEXT("%s default base value"), PropertyName),
			AttributeData->GetBaseValue(),
			ExpectedDefaultValue);
	}

	return AttributeProperty && AttributeData;
}

bool TestBlueprintAssignableDelegate(FAutomationTestBase& Test, UClass* Class, const TCHAR* PropertyName)
{
	const FMulticastInlineDelegateProperty* DelegateProperty = FindFProperty<FMulticastInlineDelegateProperty>(Class, PropertyName);
	Test.TestNotNull(FString::Printf(TEXT("%s delegate exists"), PropertyName), DelegateProperty);
	Test.TestTrue(
		FString::Printf(TEXT("%s is BlueprintAssignable"), PropertyName),
		DelegateProperty && DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable));

	return DelegateProperty != nullptr;
}
}

bool FPRGASCoreTest::RunTest(const FString& Parameters)
{
	UClass* AbilitySystemComponentClass = FindProjectRiftClass(TEXT("/Script/ProjectA.PRAbilitySystemComponent"));
	UClass* AttributeSetClass = FindProjectRiftClass(TEXT("/Script/ProjectA.PRAttributeSet"));

	TestNotNull(TEXT("UPRAbilitySystemComponent class exists"), AbilitySystemComponentClass);
	TestTrue(
		TEXT("UPRAbilitySystemComponent derives from UAbilitySystemComponent"),
		AbilitySystemComponentClass && AbilitySystemComponentClass->IsChildOf(UAbilitySystemComponent::StaticClass()));

	TestNotNull(TEXT("UPRAttributeSet class exists"), AttributeSetClass);
	TestTrue(
		TEXT("UPRAttributeSet derives from UAttributeSet"),
		AttributeSetClass && AttributeSetClass->IsChildOf(UAttributeSet::StaticClass()));

	UClass* PlayerStateClass = APRPlayerState::StaticClass();
	TestTrue(
		TEXT("APRPlayerState implements AbilitySystemInterface"),
		PlayerStateClass->ImplementsInterface(UAbilitySystemInterface::StaticClass()));
	TestNotNull(
		TEXT("APRPlayerState exposes Blueprint ASC getter"),
		PlayerStateClass->FindFunctionByName(TEXT("GetProjectRiftAbilitySystemComponent")));
	TestNotNull(
		TEXT("APRPlayerState exposes Blueprint AttributeSet getter"),
		PlayerStateClass->FindFunctionByName(TEXT("GetAttributeSet")));

	APRPlayerState* PlayerState = NewObject<APRPlayerState>();
	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(PlayerState);
	TestNotNull(TEXT("APRPlayerState can be used as an ability system owner"), AbilitySystemOwner);

	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
	TestNotNull(TEXT("APRPlayerState owns an AbilitySystemComponent"), AbilitySystemComponent);
	TestTrue(
		TEXT("APRPlayerState owns the ProjectRift ASC subclass"),
		AbilitySystemComponent && AbilitySystemComponentClass && AbilitySystemComponent->IsA(AbilitySystemComponentClass));
	TestTrue(
		TEXT("APRPlayerState ASC is configured for replication"),
		AbilitySystemComponent && AbilitySystemComponent->GetIsReplicated());

	UObject* AttributeSet = AttributeSetClass ? FindObject<UObject>(PlayerState, TEXT("AttributeSet")) : nullptr;
	TestNotNull(TEXT("APRPlayerState owns an AttributeSet subobject"), AttributeSet);
	TestTrue(
		TEXT("APRPlayerState owns the ProjectRift AttributeSet subclass"),
		AttributeSet && AttributeSetClass && AttributeSet->IsA(AttributeSetClass));

	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("Health"), TEXT("OnRep_Health"), 100.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("MaxHealth"), TEXT("OnRep_MaxHealth"), 100.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("Shield"), TEXT("OnRep_Shield"), 50.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("MaxShield"), TEXT("OnRep_MaxShield"), 50.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("Energy"), TEXT("OnRep_Energy"), 100.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("MaxEnergy"), TEXT("OnRep_MaxEnergy"), 100.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("AttackPower"), TEXT("OnRep_AttackPower"), 10.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("MoveSpeed"), TEXT("OnRep_MoveSpeed"), 600.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("CooldownReduction"), TEXT("OnRep_CooldownReduction"), 0.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("HealingPower"), TEXT("OnRep_HealingPower"), 0.0f);
	TestGameplayAttributeProperty(*this, AttributeSetClass, TEXT("PollutionResistance"), TEXT("OnRep_PollutionResistance"), 0.0f);

	UClass* CharacterClass = APRCharacter::StaticClass();
	TestBlueprintAssignableDelegate(*this, CharacterClass, TEXT("OnHealthChanged"));
	TestBlueprintAssignableDelegate(*this, CharacterClass, TEXT("OnShieldChanged"));
	TestBlueprintAssignableDelegate(*this, CharacterClass, TEXT("OnEnergyChanged"));

	return true;
}

#endif
