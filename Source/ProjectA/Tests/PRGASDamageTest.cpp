#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffect.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASDamageTest, "ProjectRift.GAS.Damage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindProjectRiftClassForDamageTest(const TCHAR* ClassPath)
{
	return FindObject<UClass>(nullptr, ClassPath);
}

bool ApplyDamage(FAutomationTestBase& Test, APRPlayerState* TargetPlayerState, const float DamageAmount)
{
	UClass* DamageEffectClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));
	Test.TestNotNull(TEXT("GE_Damage class exists"), DamageEffectClass);
	if (!DamageEffectClass)
	{
		return false;
	}

	const FGameplayTag DamageTag = ProjectRiftGameplayTags::Data_Damage;
	Test.TestTrue(TEXT("Data.Damage SetByCaller tag exists"), DamageTag.IsValid());
	if (!DamageTag.IsValid() || !TargetPlayerState)
	{
		return false;
	}

	UPRAbilitySystemComponent* TargetASC = TargetPlayerState->GetProjectRiftAbilitySystemComponent();
	Test.TestNotNull(TEXT("Target ASC exists"), TargetASC);
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
	EffectContext.AddSourceObject(TargetPlayerState);
	FGameplayEffectSpecHandle DamageSpecHandle = TargetASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	Test.TestTrue(TEXT("GE_Damage spec can be created"), DamageSpecHandle.IsValid());
	if (!DamageSpecHandle.IsValid())
	{
		return false;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageAmount);
	TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
	return true;
}
}

bool FPRGASDamageTest::RunTest(const FString& Parameters)
{
	UClass* DamageEffectClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));
	TestNotNull(TEXT("GE_Damage class exists"), DamageEffectClass);
	TestTrue(
		TEXT("GE_Damage derives from UGameplayEffect"),
		DamageEffectClass && DamageEffectClass->IsChildOf(UGameplayEffect::StaticClass()));

	UClass* AttributeSetClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRAttributeSet"));
	const FStructProperty* DamageProperty = AttributeSetClass
		? FindFProperty<FStructProperty>(AttributeSetClass, TEXT("Damage"))
		: nullptr;
	TestNotNull(TEXT("Damage meta attribute exists"), DamageProperty);
	TestTrue(
		TEXT("Damage uses FGameplayAttributeData"),
		DamageProperty && DamageProperty->Struct == FGameplayAttributeData::StaticStruct());
	TestFalse(
		TEXT("Damage meta attribute is not replicated"),
		DamageProperty && DamageProperty->HasAnyPropertyFlags(CPF_Net));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	TStrongObjectPtr<APRPlayerState> TargetPlayerState{ World->SpawnActor<APRPlayerState>() };
	TestNotNull(TEXT("Target PlayerState spawned"), TargetPlayerState.Get());
	if (!TargetPlayerState)
	{
		return false;
	}

	UPRAbilitySystemComponent* TargetASC = TargetPlayerState->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Target ASC exists"), TargetASC);
	TestNotNull(TEXT("Target AttributeSet exists"), TargetAttributes);
	if (!TargetASC || !TargetAttributes)
	{
		return false;
	}

	TargetASC->InitAbilityActorInfo(TargetPlayerState.Get(), TargetPlayerState.Get());

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("30 damage GE applies"), ApplyDamage(*this, TargetPlayerState.Get(), 30.0f));
	TestEqual(TEXT("Damage is absorbed by shield first"), TargetAttributes->GetShield(), 20.0f);
	TestEqual(TEXT("Health stays full while shield remains"), TargetAttributes->GetHealth(), 100.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("80 damage GE applies"), ApplyDamage(*this, TargetPlayerState.Get(), 80.0f));
	TestEqual(TEXT("Shield is depleted before health"), TargetAttributes->GetShield(), 0.0f);
	TestEqual(TEXT("Overflow damage lowers health"), TargetAttributes->GetHealth(), 70.0f);

	TargetAttributes->SetHealth(40.0f);
	TargetAttributes->SetShield(0.0f);
	TestTrue(TEXT("Lethal damage GE applies"), ApplyDamage(*this, TargetPlayerState.Get(), 250.0f));
	TestEqual(TEXT("Health is clamped to zero"), TargetAttributes->GetHealth(), 0.0f);

	const FStructProperty* RuntimeDamageProperty = FindFProperty<FStructProperty>(TargetAttributes->GetClass(), TEXT("Damage"));
	const FGameplayAttributeData* RuntimeDamage = RuntimeDamageProperty
		? RuntimeDamageProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(TargetAttributes)
		: nullptr;
	TestNotNull(TEXT("Runtime Damage data exists"), RuntimeDamage);
	if (RuntimeDamage)
	{
		TestEqual(TEXT("Damage meta attribute resets after execution"), RuntimeDamage->GetCurrentValue(), 0.0f);
	}

	return true;
}

#endif
