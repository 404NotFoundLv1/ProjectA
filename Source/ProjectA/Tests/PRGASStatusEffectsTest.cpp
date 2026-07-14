#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRGASStatusEffectsTest,
	"ProjectRift.GAS.StatusEffects.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindStatusTestClass(const TCHAR* ClassPath)
{
	return FindObject<UClass>(nullptr, ClassPath);
}

void TestStatusEffectClass(
	FAutomationTestBase& Test,
	const TCHAR* Label,
	const TCHAR* ClassPath,
	const UClass* StatusBaseClass)
{
	UClass* EffectClass = FindStatusTestClass(ClassPath);
	Test.TestNotNull(*FString::Printf(TEXT("%s class exists"), Label), EffectClass);
	Test.TestTrue(
		*FString::Printf(TEXT("%s derives from status base"), Label),
		EffectClass && StatusBaseClass && EffectClass->IsChildOf(StatusBaseClass));
}

void TestAbilityStatusDefaults(
	FAutomationTestBase& Test,
	const TCHAR* AbilityLabel,
	const TCHAR* AbilityClassPath,
	const int32 ExpectedCount,
	const TCHAR* ExpectedEffectClassPath = nullptr,
	const float ExpectedMagnitude = 0.0f,
	const float ExpectedDuration = 0.0f)
{
	UClass* AbilityClass = FindStatusTestClass(AbilityClassPath);
	Test.TestNotNull(*FString::Printf(TEXT("%s class exists"), AbilityLabel), AbilityClass);
	if (!AbilityClass)
	{
		return;
	}

	const UGameplayAbility* Ability = Cast<UGameplayAbility>(AbilityClass->GetDefaultObject());
	Test.TestNotNull(*FString::Printf(TEXT("%s default object exists"), AbilityLabel), Ability);
	if (Ability)
	{
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false);
		const FStructProperty* BlockedTagsProperty = FindFProperty<FStructProperty>(AbilityClass, TEXT("ActivationBlockedTags"));
		const FGameplayTagContainer* BlockedTags = BlockedTagsProperty
			? BlockedTagsProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Ability)
			: nullptr;
		Test.TestTrue(
			*FString::Printf(TEXT("%s is blocked while stunned"), AbilityLabel),
			StunnedTag.IsValid() && BlockedTags && BlockedTags->HasTagExact(StunnedTag));
	}

	const FArrayProperty* DefinitionsProperty = FindFProperty<FArrayProperty>(AbilityClass, TEXT("TargetStatusEffects"));
	Test.TestNotNull(*FString::Printf(TEXT("%s exposes TargetStatusEffects"), AbilityLabel), DefinitionsProperty);
	if (!DefinitionsProperty)
	{
		return;
	}

	const FStructProperty* DefinitionProperty = CastField<FStructProperty>(DefinitionsProperty->Inner);
	Test.TestNotNull(TEXT("TargetStatusEffects contains status definition structs"), DefinitionProperty);
	if (!DefinitionProperty)
	{
		return;
	}

	const UObject* AbilityCDO = AbilityClass->GetDefaultObject();
	FScriptArrayHelper Definitions(DefinitionsProperty, DefinitionsProperty->ContainerPtrToValuePtr<void>(AbilityCDO));
	Test.TestEqual(*FString::Printf(TEXT("%s status count"), AbilityLabel), Definitions.Num(), ExpectedCount);
	if (ExpectedCount != 1 || Definitions.Num() != 1)
	{
		return;
	}

	const void* Definition = Definitions.GetRawPtr(0);
	const FClassProperty* EffectClassProperty = FindFProperty<FClassProperty>(DefinitionProperty->Struct, TEXT("EffectClass"));
	const FFloatProperty* MagnitudeProperty = FindFProperty<FFloatProperty>(DefinitionProperty->Struct, TEXT("Magnitude"));
	const FFloatProperty* DurationProperty = FindFProperty<FFloatProperty>(DefinitionProperty->Struct, TEXT("DurationSeconds"));
	Test.TestNotNull(TEXT("Status definition exposes EffectClass"), EffectClassProperty);
	Test.TestNotNull(TEXT("Status definition exposes Magnitude"), MagnitudeProperty);
	Test.TestNotNull(TEXT("Status definition exposes DurationSeconds"), DurationProperty);
	if (!EffectClassProperty || !MagnitudeProperty || !DurationProperty)
	{
		return;
	}

	const UClass* ActualEffectClass = Cast<UClass>(EffectClassProperty->GetObjectPropertyValue(
		EffectClassProperty->ContainerPtrToValuePtr<void>(Definition)));
	Test.TestEqual<const UClass*>(
		*FString::Printf(TEXT("%s status effect class"), AbilityLabel),
		ActualEffectClass,
		FindStatusTestClass(ExpectedEffectClassPath));
	Test.TestTrue(
		*FString::Printf(TEXT("%s status magnitude"), AbilityLabel),
		FMath::IsNearlyEqual(MagnitudeProperty->GetPropertyValue_InContainer(Definition), ExpectedMagnitude));
	Test.TestTrue(
		*FString::Printf(TEXT("%s status duration"), AbilityLabel),
		FMath::IsNearlyEqual(DurationProperty->GetPropertyValue_InContainer(Definition), ExpectedDuration));
}
}

bool FPRGASStatusEffectsTest::RunTest(const FString& Parameters)
{
	UClass* StatusBaseClass = FindStatusTestClass(TEXT("/Script/ProjectA.PRStatusGameplayEffect"));
	TestNotNull(TEXT("Status GameplayEffect base exists"), StatusBaseClass);
	TestTrue(
		TEXT("Status base derives from GameplayEffect"),
		StatusBaseClass && StatusBaseClass->IsChildOf(UGameplayEffect::StaticClass()));

	TestStatusEffectClass(
		*this,
		TEXT("Pollution status"),
		TEXT("/Script/ProjectA.PRPollutionStatusGameplayEffect"),
		StatusBaseClass);
	TestStatusEffectClass(
		*this,
		TEXT("Slow status"),
		TEXT("/Script/ProjectA.PRSlowStatusGameplayEffect"),
		StatusBaseClass);
	TestStatusEffectClass(
		*this,
		TEXT("Stun status"),
		TEXT("/Script/ProjectA.PRStunStatusGameplayEffect"),
		StatusBaseClass);

	UClass* CombatEffectLibraryClass = FindStatusTestClass(TEXT("/Script/ProjectA.PRCombatEffectLibrary"));
	TestNotNull(TEXT("Shared combat effect library exists"), CombatEffectLibraryClass);

	TestAbilityStatusDefaults(
		*this,
		TEXT("Primary attack"),
		TEXT("/Script/ProjectA.GA_PrimaryAttack"),
		0);
	TestAbilityStatusDefaults(
		*this,
		TEXT("Assault charge"),
		TEXT("/Script/ProjectA.GA_AssaultCharge"),
		1,
		TEXT("/Script/ProjectA.PRSlowStatusGameplayEffect"),
		0.70f,
		3.0f);
	TestAbilityStatusDefaults(
		*this,
		TEXT("Assault blast"),
		TEXT("/Script/ProjectA.GA_AssaultBlast"),
		1,
		TEXT("/Script/ProjectA.PRStunStatusGameplayEffect"),
		0.0f,
		1.25f);
	TestAbilityStatusDefaults(
		*this,
		TEXT("Enemy melee"),
		TEXT("/Script/ProjectA.GA_EnemyMeleeAttack"),
		1,
		TEXT("/Script/ProjectA.PRPollutionStatusGameplayEffect"),
		2.0f,
		5.0f);

	return true;
}

#endif
