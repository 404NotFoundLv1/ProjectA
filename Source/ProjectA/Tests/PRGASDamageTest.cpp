#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
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

FGameplayTag RequestDamageTestTag(const TCHAR* TagName)
{
	return FGameplayTag::RequestGameplayTag(FName(TagName), false);
}

bool ApplyDamage(
	FAutomationTestBase& Test,
	APRPlayerState* SourcePlayerState,
	APRPlayerState* TargetPlayerState,
	const float DamageAmount,
	const TArray<FGameplayTag>& DamageTypeTags = {})
{
	UClass* DamageEffectClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));
	Test.TestNotNull(TEXT("GE_Damage class exists"), DamageEffectClass);
	if (!DamageEffectClass || !TargetPlayerState)
	{
		return false;
	}

	const FGameplayTag DamageTag = ProjectRiftGameplayTags::Data_Damage;
	Test.TestTrue(TEXT("Data.Damage SetByCaller tag exists"), DamageTag.IsValid());
	if (!DamageTag.IsValid())
	{
		return false;
	}

	UPRAbilitySystemComponent* TargetASC = TargetPlayerState->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* SourceASC = SourcePlayerState
		? SourcePlayerState->GetProjectRiftAbilitySystemComponent()
		: TargetASC;
	Test.TestNotNull(TEXT("Source ASC exists"), SourceASC);
	Test.TestNotNull(TEXT("Target ASC exists"), TargetASC);
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourcePlayerState ? static_cast<UObject*>(SourcePlayerState) : TargetPlayerState);
	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);
	Test.TestTrue(TEXT("GE_Damage spec can be created"), DamageSpecHandle.IsValid());
	if (!DamageSpecHandle.IsValid())
	{
		return false;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageTag, DamageAmount);
	for (const FGameplayTag DamageTypeTag : DamageTypeTags)
	{
		if (DamageTypeTag.IsValid())
		{
			DamageSpecHandle.Data->AddDynamicAssetTag(DamageTypeTag);
		}
	}
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	return true;
}

void TestFloatNear(FAutomationTestBase& Test, const TCHAR* What, const float Actual, const float Expected)
{
	Test.TestTrue(What, FMath::IsNearlyEqual(Actual, Expected, KINDA_SMALL_NUMBER));
}
}

bool FPRGASDamageTest::RunTest(const FString& Parameters)
{
	UClass* DamageEffectClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));
	UClass* DamageExecutionClass = FindProjectRiftClassForDamageTest(TEXT("/Script/ProjectA.PRDamageExecutionCalculation"));
	TestNotNull(TEXT("GE_Damage class exists"), DamageEffectClass);
	TestTrue(
		TEXT("GE_Damage derives from UGameplayEffect"),
		DamageEffectClass && DamageEffectClass->IsChildOf(UGameplayEffect::StaticClass()));
	TestNotNull(TEXT("Unified damage Execution class exists"), DamageExecutionClass);

	const UGameplayEffect* DamageEffect = DamageEffectClass ? Cast<UGameplayEffect>(DamageEffectClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("GE_Damage default object exists"), DamageEffect);
	TestEqual(TEXT("GE_Damage has exactly one Execution"), DamageEffect ? DamageEffect->Executions.Num() : 0, 1);
	if (DamageEffect && DamageEffect->Executions.Num() == 1)
	{
		TestEqual(
			TEXT("GE_Damage uses the unified damage Execution"),
			DamageEffect->Executions[0].CalculationClass.Get(),
			DamageExecutionClass);
	}

	const FGameplayTag PhysicalDamageTag = RequestDamageTestTag(TEXT("Damage.Type.Physical"));
	const FGameplayTag PollutionDamageTag = RequestDamageTestTag(TEXT("Damage.Type.Pollution"));
	TestTrue(TEXT("Physical damage type tag exists"), PhysicalDamageTag.IsValid());
	TestTrue(TEXT("Pollution damage type tag exists"), PollutionDamageTag.IsValid());

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

	TStrongObjectPtr<APRPlayerState> SourcePlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRPlayerState> TargetPlayerState{ World->SpawnActor<APRPlayerState>() };
	TestNotNull(TEXT("Source PlayerState spawned"), SourcePlayerState.Get());
	TestNotNull(TEXT("Target PlayerState spawned"), TargetPlayerState.Get());
	if (!SourcePlayerState || !TargetPlayerState)
	{
		return false;
	}

	UPRAbilitySystemComponent* SourceASC = SourcePlayerState->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* TargetASC = TargetPlayerState->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* SourceAttributes = SourcePlayerState->GetAttributeSet();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Source ASC exists"), SourceASC);
	TestNotNull(TEXT("Target ASC exists"), TargetASC);
	TestNotNull(TEXT("Source AttributeSet exists"), SourceAttributes);
	TestNotNull(TEXT("Target AttributeSet exists"), TargetAttributes);
	if (!SourceASC || !TargetASC || !SourceAttributes || !TargetAttributes)
	{
		return false;
	}

	SourceASC->InitAbilityActorInfo(SourcePlayerState.Get(), SourcePlayerState.Get());
	TargetASC->InitAbilityActorInfo(TargetPlayerState.Get(), TargetPlayerState.Get());
	SourceAttributes->SetAttackPower(10.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("30 physical damage GE applies"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), 30.0f, { PhysicalDamageTag }));
	TestFloatNear(*this, TEXT("AttackPower scales physical damage before shield"), TargetAttributes->GetShield(), 17.0f);
	TestFloatNear(*this, TEXT("Health stays full while shield remains"), TargetAttributes->GetHealth(), 100.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("Untyped legacy damage GE applies"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), 30.0f));
	TestFloatNear(*this, TEXT("Untyped damage defaults to physical"), TargetAttributes->GetShield(), 17.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TargetAttributes->SetPollutionResistance(0.25f);
	TestTrue(TEXT("80 pollution damage GE applies"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), 80.0f, { PollutionDamageTag }));
	TestFloatNear(*this, TEXT("Pollution depletes shield before health"), TargetAttributes->GetShield(), 0.0f);
	TestFloatNear(*this, TEXT("Pollution resistance reduces overflow damage"), TargetAttributes->GetHealth(), 84.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TargetAttributes->SetPollutionResistance(1.0f);
	TestTrue(TEXT("Pollution resistance cap path applies"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), 100.0f, { PollutionDamageTag }));
	TestFloatNear(*this, TEXT("Pollution resistance caps at eighty percent"), TargetAttributes->GetShield(), 28.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TargetAttributes->SetPollutionResistance(0.0f);
	TestTrue(TEXT("Self-sourced environmental damage applies"), ApplyDamage(*this, TargetPlayerState.Get(), TargetPlayerState.Get(), 30.0f, { PhysicalDamageTag }));
	TestFloatNear(*this, TEXT("Self-sourced damage ignores AttackPower"), TargetAttributes->GetShield(), 20.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("Negative damage spec is safely processed"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), -10.0f, { PhysicalDamageTag }));
	TestFloatNear(*this, TEXT("Negative damage does not change shield"), TargetAttributes->GetShield(), 50.0f);
	TestFloatNear(*this, TEXT("Negative damage does not change health"), TargetAttributes->GetHealth(), 100.0f);

	if (PhysicalDamageTag.IsValid() && PollutionDamageTag.IsValid())
	{
		TargetAttributes->SetHealth(100.0f);
		TargetAttributes->SetShield(50.0f);
		TestTrue(TEXT("Ambiguous typed damage spec is safely processed"), ApplyDamage(
			*this,
			SourcePlayerState.Get(),
			TargetPlayerState.Get(),
			30.0f,
			{ PhysicalDamageTag, PollutionDamageTag }));
		TestFloatNear(*this, TEXT("Ambiguous typed damage fails closed"), TargetAttributes->GetShield(), 50.0f);
	}

	TargetAttributes->SetHealth(40.0f);
	TargetAttributes->SetShield(0.0f);
	TestTrue(TEXT("Lethal damage GE applies"), ApplyDamage(*this, SourcePlayerState.Get(), TargetPlayerState.Get(), 250.0f, { PhysicalDamageTag }));
	TestFloatNear(*this, TEXT("Health is clamped to zero"), TargetAttributes->GetHealth(), 0.0f);

	const FStructProperty* RuntimeDamageProperty = FindFProperty<FStructProperty>(TargetAttributes->GetClass(), TEXT("Damage"));
	const FGameplayAttributeData* RuntimeDamage = RuntimeDamageProperty
		? RuntimeDamageProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(TargetAttributes)
		: nullptr;
	TestNotNull(TEXT("Runtime Damage data exists"), RuntimeDamage);
	if (RuntimeDamage)
	{
		TestFloatNear(*this, TEXT("Damage meta attribute resets after execution"), RuntimeDamage->GetCurrentValue(), 0.0f);
	}

	return true;
}

#endif
