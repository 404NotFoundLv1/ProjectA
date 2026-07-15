#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UI/PRWeaponHUDWidget.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#include <type_traits>
#include <utility>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRHitConfirmationTest,
	"ProjectRift.CombatFeedback.HitConfirmation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRHitConfirmationSourceIsolationTest,
	"ProjectRift.CombatFeedback.HitConfirmationSourceIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
template <typename PropertyType>
PropertyType* RequireProperty(
	FAutomationTestBase& Test,
	const UStruct* Owner,
	const FName PropertyName,
	const TCHAR* OwnerLabel)
{
	PropertyType* Property = Owner ? FindFProperty<PropertyType>(Owner, PropertyName) : nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("%s exposes %s"), OwnerLabel, *PropertyName.ToString()),
		Property);
	return Property;
}

void RequireTransientBlueprintReadOnly(
	FAutomationTestBase& Test,
	const FProperty* Property,
	const TCHAR* PropertyLabel)
{
	if (!Property)
	{
		return;
	}
	Test.TestTrue(
		FString::Printf(TEXT("%s is transient"), PropertyLabel),
		Property->HasAnyPropertyFlags(CPF_Transient));
	Test.TestTrue(
		FString::Printf(TEXT("%s is Blueprint visible"), PropertyLabel),
		Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
	Test.TestTrue(
		FString::Printf(TEXT("%s is Blueprint read-only"), PropertyLabel),
		Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
}

UFunction* RequireFunction(
	FAutomationTestBase& Test,
	const UClass* Owner,
	const FName FunctionName,
	const TCHAR* OwnerLabel)
{
	UFunction* Function = Owner ? Owner->FindFunctionByName(FunctionName) : nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("%s exposes %s"), OwnerLabel, *FunctionName.ToString()),
		Function);
	return Function;
}

template <typename ReturnPropertyType, typename ReturnType>
bool InvokeNoArgGetter(UObject* Object, UFunction* Function, ReturnType& OutValue)
{
	if (!Object || !Function)
	{
		return false;
	}

	FStructOnScope Parameters(Function);
	ReturnPropertyType* ReturnProperty = nullptr;
	int32 ReturnPropertyCount = 0;
	int32 InputPropertyCount = 0;
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnProperty = CastField<ReturnPropertyType>(*It);
			++ReturnPropertyCount;
		}
		else if (It->HasAnyPropertyFlags(CPF_Parm))
		{
			++InputPropertyCount;
		}
	}
	if (!ReturnProperty || ReturnPropertyCount != 1 || InputPropertyCount != 0)
	{
		return false;
	}

	Object->ProcessEvent(Function, Parameters.GetStructMemory());
	OutValue = ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory());
	return true;
}

bool InvokeAdvance(UPRWeaponHUDWidget* HUD, UFunction* Function, const float DeltaSeconds)
{
	if (!HUD || !Function)
	{
		return false;
	}

	FStructOnScope Parameters(Function);
	FFloatProperty* DeltaProperty = nullptr;
	int32 InputPropertyCount = 0;
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			DeltaProperty = CastField<FFloatProperty>(*It);
			++InputPropertyCount;
		}
	}
	if (!DeltaProperty || InputPropertyCount != 1)
	{
		return false;
	}

	DeltaProperty->SetPropertyValue_InContainer(Parameters.GetStructMemory(), DeltaSeconds);
	HUD->ProcessEvent(Function, Parameters.GetStructMemory());
	return true;
}

bool InvokeCameraShakeScale(UFunction* Function, const bool bReduceCameraShake, float& OutScale)
{
	if (!Function)
	{
		return false;
	}

	UObject* FunctionOwner = Function->GetOuterUClass()
		? Function->GetOuterUClass()->GetDefaultObject()
		: nullptr;
	if (!FunctionOwner)
	{
		return false;
	}

	FStructOnScope Parameters(Function);
	FBoolProperty* InputProperty = nullptr;
	FFloatProperty* ReturnProperty = nullptr;
	int32 InputPropertyCount = 0;
	int32 ReturnPropertyCount = 0;
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnProperty = CastField<FFloatProperty>(*It);
			++ReturnPropertyCount;
		}
		else if (It->HasAnyPropertyFlags(CPF_Parm))
		{
			InputProperty = CastField<FBoolProperty>(*It);
			++InputPropertyCount;
		}
	}
	if (!InputProperty || !ReturnProperty || InputPropertyCount != 1 || ReturnPropertyCount != 1)
	{
		return false;
	}

	InputProperty->SetPropertyValue_InContainer(Parameters.GetStructMemory(), bReduceCameraShake);
	FunctionOwner->ProcessEvent(Function, Parameters.GetStructMemory());
	OutScale = ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory());
	return true;
}

struct FObservedDamageNumber
{
	float Amount = 0.0f;
	FLinearColor Color = FLinearColor::Transparent;
	float RemainingSeconds = 0.0f;
};

TArray<FObservedDamageNumber> ObserveDamageNumbers(
	FAutomationTestBase& Test,
	UPRWeaponHUDWidget* HUD,
	FArrayProperty* ActiveDamageNumbersProperty,
	UScriptStruct* DamageNumberStruct)
{
	TArray<FObservedDamageNumber> Observed;
	if (!HUD || !ActiveDamageNumbersProperty || !DamageNumberStruct)
	{
		return Observed;
	}

	const FStructProperty* InnerStructProperty = CastField<FStructProperty>(ActiveDamageNumbersProperty->Inner);
	Test.TestTrue(
		TEXT("ActiveDamageNumbers contains FPRHUDDamageNumber entries"),
		InnerStructProperty && InnerStructProperty->Struct == DamageNumberStruct);
	if (!InnerStructProperty || InnerStructProperty->Struct != DamageNumberStruct)
	{
		return Observed;
	}

	FFloatProperty* AmountProperty = RequireProperty<FFloatProperty>(
		Test, DamageNumberStruct, TEXT("Amount"), TEXT("FPRHUDDamageNumber"));
	FStructProperty* ColorProperty = RequireProperty<FStructProperty>(
		Test, DamageNumberStruct, TEXT("Color"), TEXT("FPRHUDDamageNumber"));
	FFloatProperty* RemainingProperty = RequireProperty<FFloatProperty>(
		Test, DamageNumberStruct, TEXT("RemainingSeconds"), TEXT("FPRHUDDamageNumber"));
	if (!AmountProperty || !ColorProperty || !RemainingProperty)
	{
		return Observed;
	}
	Test.TestEqual<const UScriptStruct*>(
		TEXT("FPRHUDDamageNumber Color uses FLinearColor"),
		ColorProperty->Struct.Get(),
		TBaseStructure<FLinearColor>::Get());
	if (ColorProperty->Struct != TBaseStructure<FLinearColor>::Get())
	{
		return Observed;
	}

	FScriptArrayHelper ArrayHelper(
		ActiveDamageNumbersProperty,
		ActiveDamageNumbersProperty->ContainerPtrToValuePtr<void>(HUD));
	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* Entry = ArrayHelper.GetRawPtr(Index);
		FObservedDamageNumber& Number = Observed.AddDefaulted_GetRef();
		Number.Amount = AmountProperty->GetPropertyValue_InContainer(Entry);
		Number.Color = *ColorProperty->ContainerPtrToValuePtr<FLinearColor>(Entry);
		Number.RemainingSeconds = RemainingProperty->GetPropertyValue_InContainer(Entry);
	}
	return Observed;
}

const FObservedDamageNumber* FindDamageNumber(
	const TArray<FObservedDamageNumber>& Numbers,
	const float Amount)
{
	return Numbers.FindByPredicate([Amount](const FObservedDamageNumber& Number)
	{
		return FMath::IsNearlyEqual(Number.Amount, Amount, KINDA_SMALL_NUMBER);
	});
}

template <typename ControllerType, typename = void>
struct THasPublicHitConfirmationSender : std::false_type
{
};

template <typename ControllerType>
struct THasPublicHitConfirmationSender<
	ControllerType,
	std::void_t<decltype(std::declval<ControllerType&>().SendHitConfirmationToOwner(
		std::declval<const FPRHitConfirmation&>()))>>
	: std::is_same<
		decltype(std::declval<ControllerType&>().SendHitConfirmationToOwner(
			std::declval<const FPRHitConfirmation&>())),
		void>
{
};

struct FHitConfirmationTestPlayer
{
	APRPlayerController* Controller = nullptr;
	APRPlayerState* PlayerState = nullptr;
	APRCharacter* Character = nullptr;
	UPRAbilitySystemComponent* AbilitySystem = nullptr;
	UPRAttributeSet* Attributes = nullptr;
};

FHitConfirmationTestPlayer SpawnHitConfirmationTestPlayer(
	UWorld* World,
	const FVector& Location)
{
	FHitConfirmationTestPlayer Result;
	if (!World)
	{
		return Result;
	}

	Result.PlayerState = World->SpawnActor<APRPlayerState>();
	Result.Character = World->SpawnActor<APRCharacter>(Location, FRotator::ZeroRotator);
	Result.Controller = World->SpawnActor<APRPlayerController>();
	if (Result.PlayerState && Result.Character && Result.Controller)
	{
		Result.Controller->SetPlayerState(Result.PlayerState);
		Result.Controller->Possess(Result.Character);
		Result.AbilitySystem = Result.Character->GetProjectRiftAbilitySystemComponent();
		Result.Attributes = Result.PlayerState->GetAttributeSet();
	}
	return Result;
}

void TickHitConfirmationWorld(UWorld* World, const float DurationSeconds)
{
	constexpr float StepSeconds = 0.02f;
	for (float Elapsed = 0.0f; World && Elapsed < DurationSeconds; Elapsed += StepSeconds)
	{
		++GFrameCounter;
		World->Tick(
			ELevelTick::LEVELTICK_All,
			FMath::Min(StepSeconds, DurationSeconds - Elapsed));
	}
}

int32 ReadHitConfirmationCount(
	const UObject* Controller,
	const FIntProperty* CountProperty)
{
	return Controller && CountProperty
		? CountProperty->GetPropertyValue_InContainer(Controller)
		: INDEX_NONE;
}

FPRHitConfirmation ReadLastHitConfirmation(
	const UObject* Controller,
	const FStructProperty* ConfirmationProperty)
{
	return Controller
		&& ConfirmationProperty
		&& ConfirmationProperty->Struct == FPRHitConfirmation::StaticStruct()
		? *ConfirmationProperty->ContainerPtrToValuePtr<FPRHitConfirmation>(Controller)
		: FPRHitConfirmation();
}
}

bool FPRHitConfirmationTest::RunTest(const FString& Parameters)
{
	// Source-owned confirmation RPC contract and transient observation state.
	UClass* ControllerClass = APRPlayerController::StaticClass();
	UFunction* ConfirmationRPC = RequireFunction(
		*this, ControllerClass, TEXT("ClientReceiveHitConfirmation"), TEXT("APRPlayerController"));
	if (ConfirmationRPC)
	{
		TestTrue(TEXT("Hit confirmation is a network RPC"), ConfirmationRPC->HasAnyFunctionFlags(FUNC_Net));
		TestTrue(TEXT("Hit confirmation is an owning-client RPC"), ConfirmationRPC->HasAnyFunctionFlags(FUNC_NetClient));
		TestFalse(TEXT("Hit confirmation is not a server RPC"), ConfirmationRPC->HasAnyFunctionFlags(FUNC_NetServer));
		TestFalse(TEXT("Hit confirmation is not a multicast RPC"), ConfirmationRPC->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestFalse(TEXT("Hit confirmation is deliberately unreliable"), ConfirmationRPC->HasAnyFunctionFlags(FUNC_NetReliable));

		FStructProperty* ConfirmationParameter = nullptr;
		int32 ConfirmationParameterCount = 0;
		for (TFieldIterator<FProperty> It(ConfirmationRPC); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ConfirmationParameter = CastField<FStructProperty>(*It);
				++ConfirmationParameterCount;
			}
		}
		TestTrue(
			TEXT("Hit confirmation RPC accepts FPRHitConfirmation"),
			ConfirmationParameterCount == 1
				&& ConfirmationParameter
				&& ConfirmationParameter->Struct == FPRHitConfirmation::StaticStruct());
	}

	FStructProperty* LastConfirmationProperty = RequireProperty<FStructProperty>(
		*this, ControllerClass, TEXT("LastHitConfirmation"), TEXT("APRPlayerController"));
	FIntProperty* SentCountProperty = RequireProperty<FIntProperty>(
		*this, ControllerClass, TEXT("HitConfirmationSentCount"), TEXT("APRPlayerController"));
	TestEqual<const UScriptStruct*>(
		TEXT("LastHitConfirmation uses exactly FPRHitConfirmation before runtime observation"),
		LastConfirmationProperty ? LastConfirmationProperty->Struct.Get() : nullptr,
		FPRHitConfirmation::StaticStruct());
	const bool bLastConfirmationTypeIsSafe = LastConfirmationProperty
		&& LastConfirmationProperty->Struct == FPRHitConfirmation::StaticStruct();
	if (!bLastConfirmationTypeIsSafe || !SentCountProperty)
	{
		return false;
	}
	FIntProperty* ReceivedCountProperty = RequireProperty<FIntProperty>(
		*this, ControllerClass, TEXT("HitConfirmationReceivedCount"), TEXT("APRPlayerController"));
	if (LastConfirmationProperty)
	{
		TestEqual<const UScriptStruct*>(
			TEXT("LastHitConfirmation uses FPRHitConfirmation"),
			LastConfirmationProperty->Struct.Get(),
			FPRHitConfirmation::StaticStruct());
	}
	RequireTransientBlueprintReadOnly(*this, LastConfirmationProperty, TEXT("LastHitConfirmation"));
	RequireTransientBlueprintReadOnly(*this, SentCountProperty, TEXT("HitConfirmationSentCount"));
	RequireTransientBlueprintReadOnly(*this, ReceivedCountProperty, TEXT("HitConfirmationReceivedCount"));

	// HUD contract and deterministic lifetime progression.
	UClass* HUDClass = UPRWeaponHUDWidget::StaticClass();
	UFunction* AdvanceFunction = RequireFunction(
		*this, HUDClass, TEXT("AdvanceHitFeedback"), TEXT("UPRWeaponHUDWidget"));
	UFunction* MarkerActiveFunction = RequireFunction(
		*this, HUDClass, TEXT("IsHitMarkerActive"), TEXT("UPRWeaponHUDWidget"));
	UFunction* DamageCountFunction = RequireFunction(
		*this, HUDClass, TEXT("GetActiveDamageNumberCount"), TEXT("UPRWeaponHUDWidget"));
	UFunction* MarkerRemainingFunction = RequireFunction(
		*this, HUDClass, TEXT("GetHitMarkerRemainingSeconds"), TEXT("UPRWeaponHUDWidget"));
	if (AdvanceFunction)
	{
		TestTrue(TEXT("AdvanceHitFeedback is Blueprint callable"), AdvanceFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}
	for (UFunction* Getter : { MarkerActiveFunction, DamageCountFunction, MarkerRemainingFunction })
	{
		if (Getter)
		{
			TestTrue(TEXT("Hit feedback query is Blueprint pure"), Getter->HasAnyFunctionFlags(FUNC_BlueprintPure));
		}
	}

	UScriptStruct* DamageNumberStruct = FindObject<UScriptStruct>(
		nullptr,
		TEXT("/Script/ProjectA.PRHUDDamageNumber"));
	TestNotNull(TEXT("FPRHUDDamageNumber is registered"), DamageNumberStruct);
	FArrayProperty* ActiveDamageNumbersProperty = RequireProperty<FArrayProperty>(
		*this, HUDClass, TEXT("ActiveDamageNumbers"), TEXT("UPRWeaponHUDWidget"));
	RequireTransientBlueprintReadOnly(*this, ActiveDamageNumbersProperty, TEXT("ActiveDamageNumbers"));
	const FStructProperty* DamageNumberInnerProperty = ActiveDamageNumbersProperty
		? CastField<FStructProperty>(ActiveDamageNumbersProperty->Inner)
		: nullptr;
	const bool bHasDamageNumberArrayContract = DamageNumberStruct
		&& DamageNumberInnerProperty
		&& DamageNumberInnerProperty->Struct == DamageNumberStruct;
	if (ActiveDamageNumbersProperty && DamageNumberStruct)
	{
		TestTrue(
			TEXT("ActiveDamageNumbers contains FPRHUDDamageNumber entries"),
			bHasDamageNumberArrayContract);
	}

	const bool bHasHUDRuntimeContract = AdvanceFunction
		&& MarkerActiveFunction
		&& DamageCountFunction
		&& MarkerRemainingFunction
		&& ActiveDamageNumbersProperty
		&& bHasDamageNumberArrayContract;
	UPRWeaponHUDWidget* HUD = bHasHUDRuntimeContract
		? NewObject<UPRWeaponHUDWidget>()
		: nullptr;
	if (bHasHUDRuntimeContract)
	{
		TestNotNull(TEXT("Weapon HUD can be instantiated for deterministic feedback testing"), HUD);
	}
	if (HUD)
	{
		FPRHitConfirmation SplitConfirmation;
		SplitConfirmation.ShieldDamage = 7.0f;
		SplitConfirmation.HealthDamage = 5.0f;
		SplitConfirmation.bShieldBroken = true;
		HUD->PushHitConfirmation(SplitConfirmation);

		bool bMarkerActive = false;
		float MarkerRemaining = 0.0f;
		int32 DamageNumberCount = 0;
		TestTrue(TEXT("IsHitMarkerActive has a bool return value"), InvokeNoArgGetter<FBoolProperty>(HUD, MarkerActiveFunction, bMarkerActive));
		TestTrue(TEXT("Split confirmation activates the hit marker"), bMarkerActive);
		TestTrue(TEXT("GetHitMarkerRemainingSeconds has a float return value"), InvokeNoArgGetter<FFloatProperty>(HUD, MarkerRemainingFunction, MarkerRemaining));
		TestTrue(TEXT("Hit marker starts at 0.15 seconds"), FMath::IsNearlyEqual(MarkerRemaining, 0.15f, KINDA_SMALL_NUMBER));
		TestTrue(TEXT("GetActiveDamageNumberCount has an int return value"), InvokeNoArgGetter<FIntProperty>(HUD, DamageCountFunction, DamageNumberCount));
		TestEqual(TEXT("Split shield and health confirmation creates two damage numbers"), DamageNumberCount, 2);

		const TArray<FObservedDamageNumber> SplitNumbers = ObserveDamageNumbers(
			*this, HUD, ActiveDamageNumbersProperty, DamageNumberStruct);
		const FObservedDamageNumber* ShieldNumber = FindDamageNumber(SplitNumbers, 7.0f);
		const FObservedDamageNumber* HealthNumber = FindDamageNumber(SplitNumbers, 5.0f);
		TestNotNull(TEXT("Split confirmation contains its shield damage number"), ShieldNumber);
		TestNotNull(TEXT("Split confirmation contains its health damage number"), HealthNumber);
		if (ShieldNumber)
		{
			TestTrue(TEXT("Shield-break number uses bright cyan"), ShieldNumber->Color.Equals(FLinearColor(0.25f, 1.0f, 1.0f), KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Shield-break number starts at 0.65 seconds"), FMath::IsNearlyEqual(ShieldNumber->RemainingSeconds, 0.65f, KINDA_SMALL_NUMBER));
		}
		if (HealthNumber)
		{
			TestTrue(TEXT("Nonlethal health number uses white"), HealthNumber->Color.Equals(FLinearColor::White, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Health number starts at 0.65 seconds"), FMath::IsNearlyEqual(HealthNumber->RemainingSeconds, 0.65f, KINDA_SMALL_NUMBER));
		}

		TestTrue(TEXT("AdvanceHitFeedback accepts elapsed time"), InvokeAdvance(HUD, AdvanceFunction, 0.16f));
		bMarkerActive = true;
		InvokeNoArgGetter<FBoolProperty>(HUD, MarkerActiveFunction, bMarkerActive);
		TestFalse(TEXT("Hit marker expires after 0.16 seconds"), bMarkerActive);
		InvokeNoArgGetter<FIntProperty>(HUD, DamageCountFunction, DamageNumberCount);
		TestEqual(TEXT("Damage numbers remain after only 0.16 seconds"), DamageNumberCount, 2);

		TestTrue(TEXT("Damage-number lifetime accepts the remaining elapsed time"), InvokeAdvance(HUD, AdvanceFunction, 0.50f));
		InvokeNoArgGetter<FIntProperty>(HUD, DamageCountFunction, DamageNumberCount);
		TestEqual(TEXT("Damage numbers expire after cumulative 0.66 seconds"), DamageNumberCount, 0);

		FPRHitConfirmation ShieldConfirmation;
		ShieldConfirmation.ShieldDamage = 3.0f;
		HUD->PushHitConfirmation(ShieldConfirmation);
		TArray<FObservedDamageNumber> SingleNumbers = ObserveDamageNumbers(
			*this, HUD, ActiveDamageNumbersProperty, DamageNumberStruct);
		const FObservedDamageNumber* NormalShieldNumber = FindDamageNumber(SingleNumbers, 3.0f);
		TestTrue(
			TEXT("Normal shield damage uses cyan"),
			NormalShieldNumber && NormalShieldNumber->Color.Equals(FLinearColor(0.05f, 0.75f, 1.0f), KINDA_SMALL_NUMBER));
		InvokeAdvance(HUD, AdvanceFunction, 0.66f);

		FPRHitConfirmation LethalConfirmation;
		LethalConfirmation.HealthDamage = 9.0f;
		LethalConfirmation.bLethal = true;
		HUD->PushHitConfirmation(LethalConfirmation);
		SingleNumbers = ObserveDamageNumbers(*this, HUD, ActiveDamageNumbersProperty, DamageNumberStruct);
		const FObservedDamageNumber* LethalNumber = FindDamageNumber(SingleNumbers, 9.0f);
		TestTrue(
			TEXT("Lethal health damage uses red"),
			LethalNumber && LethalNumber->Color.Equals(FLinearColor::Red, KINDA_SMALL_NUMBER));
	}

	// Shared camera-shake accessibility policy.
	UFunction* CameraShakeScaleFunction = RequireFunction(
		*this,
		UPRCombatFeedbackComponent::StaticClass(),
		TEXT("ResolveCameraShakeScale"),
		TEXT("UPRCombatFeedbackComponent"));
	if (CameraShakeScaleFunction)
	{
		TestTrue(TEXT("ResolveCameraShakeScale is static"), CameraShakeScaleFunction->HasAnyFunctionFlags(FUNC_Static));
		float NormalScale = 0.0f;
		float ReducedScale = 0.0f;
		TestTrue(TEXT("Camera shake scale accepts the normal setting"), InvokeCameraShakeScale(CameraShakeScaleFunction, false, NormalScale));
		TestTrue(TEXT("Normal camera shake scale is 1.0"), FMath::IsNearlyEqual(NormalScale, 1.0f, KINDA_SMALL_NUMBER));
		TestTrue(TEXT("Camera shake scale accepts the reduced setting"), InvokeCameraShakeScale(CameraShakeScaleFunction, true, ReducedScale));
		TestTrue(TEXT("Reduced camera shake scale is 0.35"), FMath::IsNearlyEqual(ReducedScale, 0.35f, KINDA_SMALL_NUMBER));
	}

	FStructProperty* IncomingDirectionProperty = RequireProperty<FStructProperty>(
		*this,
		FPRResolvedDamage::StaticStruct(),
		TEXT("IncomingDirection"),
		TEXT("FPRResolvedDamage"));
	if (IncomingDirectionProperty)
	{
		TestEqual<const UScriptStruct*>(
			TEXT("IncomingDirection uses FVector"),
			IncomingDirectionProperty->Struct.Get(),
			TBaseStructure<FVector>::Get());
	}
	FFloatProperty* LocalPulseProperty = RequireProperty<FFloatProperty>(
		*this, HUDClass, TEXT("LocalHitPulseRemaining"), TEXT("UPRWeaponHUDWidget"));
	FStructProperty* LocalDirectionProperty = RequireProperty<FStructProperty>(
		*this, HUDClass, TEXT("LocalHitDirection"), TEXT("UPRWeaponHUDWidget"));
	RequireTransientBlueprintReadOnly(*this, LocalPulseProperty, TEXT("LocalHitPulseRemaining"));
	RequireTransientBlueprintReadOnly(*this, LocalDirectionProperty, TEXT("LocalHitDirection"));
	if (LocalDirectionProperty)
	{
		TestEqual<const UScriptStruct*>(
			TEXT("LocalHitDirection uses FVector"),
			LocalDirectionProperty->Struct.Get(),
			TBaseStructure<FVector>::Get());
	}
	const bool bIncomingDirectionTypeIsSafe = IncomingDirectionProperty
		&& IncomingDirectionProperty->Struct == TBaseStructure<FVector>::Get();
	const bool bLocalDirectionTypeIsSafe = LocalDirectionProperty
		&& LocalDirectionProperty->Struct == TBaseStructure<FVector>::Get();
	if (HUD
		&& bIncomingDirectionTypeIsSafe
		&& LocalPulseProperty
		&& bLocalDirectionTypeIsSafe)
	{
		FPRResolvedDamage IncomingDamage;
		const FVector ExpectedDirection(0.25f, -0.75f, 0.5f);
		*IncomingDirectionProperty->ContainerPtrToValuePtr<FVector>(&IncomingDamage) = ExpectedDirection;
		HUD->PushLocalHitFeedback(IncomingDamage);
		TestTrue(
			TEXT("Local hit feedback starts a positive pulse lifetime"),
			LocalPulseProperty->GetPropertyValue_InContainer(HUD) > 0.0f);
		TestTrue(
			TEXT("Local hit feedback preserves the incoming direction"),
			LocalDirectionProperty->ContainerPtrToValuePtr<FVector>(HUD)->Equals(ExpectedDirection, KINDA_SMALL_NUMBER));
	}

	return true;
}

bool FPRHitConfirmationSourceIsolationTest::RunTest(const FString& Parameters)
{
	UClass* ControllerClass = APRPlayerController::StaticClass();
	FStructProperty* LastConfirmationProperty = RequireProperty<FStructProperty>(
		*this, ControllerClass, TEXT("LastHitConfirmation"), TEXT("APRPlayerController"));
	FIntProperty* SentCountProperty = RequireProperty<FIntProperty>(
		*this, ControllerClass, TEXT("HitConfirmationSentCount"), TEXT("APRPlayerController"));

	TestTrue(
		TEXT("APRPlayerController exposes public non-Blueprint server helper SendHitConfirmationToOwner"),
		THasPublicHitConfirmationSender<APRPlayerController>::value);
	const UFunction* BlueprintSender = ControllerClass->FindFunctionByName(TEXT("SendHitConfirmationToOwner"));
	TestFalse(
		TEXT("SendHitConfirmationToOwner is not a Blueprint-callable bypass"),
		BlueprintSender && BlueprintSender->HasAnyFunctionFlags(FUNC_BlueprintCallable));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Hit-confirmation source-isolation world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Hit-confirmation source-isolation world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	FHitConfirmationTestPlayer PlayerA = SpawnHitConfirmationTestPlayer(
		World,
		FVector(0.0f, 0.0f, 0.0f));
	FHitConfirmationTestPlayer PlayerB = SpawnHitConfirmationTestPlayer(
		World,
		FVector(0.0f, 1000.0f, 0.0f));
	APREnemyCharacter* Enemy = World->SpawnActor<APREnemyCharacter>(
		FVector(500.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	UPRAbilitySystemComponent* EnemyASC = Enemy
		? Enemy->GetProjectRiftAbilitySystemComponent()
		: nullptr;
	UPRAttributeSet* EnemyAttributes = Enemy ? Enemy->GetAttributeSet() : nullptr;

	TestTrue(
		TEXT("Source player A initializes controller, PlayerState, character and ASC"),
		PlayerA.Controller && PlayerA.PlayerState && PlayerA.Character && PlayerA.AbilitySystem && PlayerA.Attributes);
	TestTrue(
		TEXT("Unrelated player B initializes controller, PlayerState, character and ASC"),
		PlayerB.Controller && PlayerB.PlayerState && PlayerB.Character && PlayerB.AbilitySystem && PlayerB.Attributes);
	TestTrue(TEXT("Confirmation target enemy initializes real GAS state"), Enemy && EnemyASC && EnemyAttributes);
	if (!PlayerA.Controller
		|| !PlayerA.Character
		|| !PlayerA.AbilitySystem
		|| !PlayerA.Attributes
		|| !PlayerB.Controller
		|| !PlayerB.Character
		|| !PlayerB.AbilitySystem
		|| !Enemy
		|| !EnemyASC
		|| !EnemyAttributes
		|| !LastConfirmationProperty
		|| !SentCountProperty)
	{
		return false;
	}

	EnemyAttributes->SetMaxHealth(50.0f);
	EnemyAttributes->SetHealth(50.0f);
	EnemyAttributes->SetMaxShield(5.0f);
	EnemyAttributes->SetShield(5.0f);
	const int32 PlayerAInitialSentCount = ReadHitConfirmationCount(PlayerA.Controller, SentCountProperty);
	const int32 PlayerBInitialSentCount = ReadHitConfirmationCount(PlayerB.Controller, SentCountProperty);

	FPRDamageRequest SourceConfirmationRequest;
	SourceConfirmationRequest.BaseDamage = 10.0f;
	SourceConfirmationRequest.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	SourceConfirmationRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetAndSource;
	TestTrue(
		TEXT("Player A structured TargetAndSource damage is accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			SourceConfirmationRequest,
			PlayerA.Character));
	TestTrue(TEXT("Player A AttackPower turns base 10 into 11 resolved damage"), FMath::IsNearlyEqual(EnemyAttributes->GetHealth(), 44.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Resolved damage consumes the enemy's five shield"), FMath::IsNearlyZero(EnemyAttributes->GetShield(), KINDA_SMALL_NUMBER));
	TestEqual(
		TEXT("Only source player A records one server-side confirmation send"),
		ReadHitConfirmationCount(PlayerA.Controller, SentCountProperty),
		PlayerAInitialSentCount + 1);
	TestEqual(
		TEXT("Unrelated player B receives no server-side confirmation send"),
		ReadHitConfirmationCount(PlayerB.Controller, SentCountProperty),
		PlayerBInitialSentCount);

	const FPRHitConfirmation Confirmation = ReadLastHitConfirmation(
		PlayerA.Controller,
		LastConfirmationProperty);
	TestTrue(TEXT("Source confirmation reports actual shield damage 5"), FMath::IsNearlyEqual(Confirmation.ShieldDamage, 5.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Source confirmation reports actual health damage 6"), FMath::IsNearlyEqual(Confirmation.HealthDamage, 6.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Source confirmation reports shield break"), Confirmation.bShieldBroken);
	TestFalse(TEXT("Source confirmation reports a nonlethal result"), Confirmation.bLethal);

	auto CapturePlayerConfirmationCounts = [
		&PlayerA,
		&PlayerB,
		SentCountProperty]()
	{
		return FIntPoint(
			ReadHitConfirmationCount(PlayerA.Controller, SentCountProperty),
			ReadHitConfirmationCount(PlayerB.Controller, SentCountProperty));
	};
	auto TestNoAdditionalPlayerConfirmation = [
		this,
		&PlayerA,
		&PlayerB,
		SentCountProperty](const TCHAR* What, const FIntPoint& BeforeCounts)
	{
		TestEqual(
			FString::Printf(TEXT("%s does not send another confirmation to player A"), What),
			ReadHitConfirmationCount(PlayerA.Controller, SentCountProperty),
			BeforeCounts.X);
		TestEqual(
			FString::Printf(TEXT("%s does not leak a confirmation to player B"), What),
			ReadHitConfirmationCount(PlayerB.Controller, SentCountProperty),
			BeforeCounts.Y);
	};

	FPRDamageRequest TargetOnlyRequest = SourceConfirmationRequest;
	TargetOnlyRequest.BaseDamage = 1.0f;
	TargetOnlyRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetOnly;
	const FIntPoint TargetOnlyBeforeCounts = CapturePlayerConfirmationCounts();
	TestTrue(
		TEXT("TargetOnly structured damage remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			TargetOnlyRequest,
			PlayerA.Character));
	TestNoAdditionalPlayerConfirmation(TEXT("TargetOnly policy"), TargetOnlyBeforeCounts);

	FPRDamageRequest NoFeedbackRequest = TargetOnlyRequest;
	NoFeedbackRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::None;
	const FIntPoint NoFeedbackBeforeCounts = CapturePlayerConfirmationCounts();
	TestTrue(
		TEXT("Policy None structured damage remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			NoFeedbackRequest,
			PlayerA.Character));
	TestNoAdditionalPlayerConfirmation(TEXT("Policy None"), NoFeedbackBeforeCounts);

	const FIntPoint LegacyBeforeCounts = CapturePlayerConfirmationCounts();
	TestTrue(
		TEXT("Legacy physical damage remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			1.0f,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			PlayerA.Character));
	TestNoAdditionalPlayerConfirmation(TEXT("Legacy damage"), LegacyBeforeCounts);

	const FPRTargetStatusEffectDefinition PollutionDefinition(
		UPRPollutionStatusGameplayEffect::StaticClass(),
		2.0f,
		0.25f);
	const FIntPoint PollutionBeforeCounts = CapturePlayerConfirmationCounts();
	TestTrue(
		TEXT("Pollution status applies for source-confirmation exclusion"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			PollutionDefinition,
			PlayerA.Character).IsValid());
	TickHitConfirmationWorld(World, 0.05f);
	TestNoAdditionalPlayerConfirmation(TEXT("Pollution immediate tick"), PollutionBeforeCounts);
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(EnemyASC);

	FPRDamageRequest FriendlyRequest = SourceConfirmationRequest;
	FriendlyRequest.BaseDamage = 1.0f;
	const FIntPoint FriendlyBeforeCounts = CapturePlayerConfirmationCounts();
	TestFalse(
		TEXT("Friendly structured damage is rejected"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			PlayerA.AbilitySystem,
			PlayerB.AbilitySystem,
			FriendlyRequest,
			PlayerA.Character));
	TestNoAdditionalPlayerConfirmation(TEXT("Friendly rejection"), FriendlyBeforeCounts);

	FPRDamageRequest InvalidRequest = SourceConfirmationRequest;
	InvalidRequest.BaseDamage = 0.0f;
	const FIntPoint InvalidBeforeCounts = CapturePlayerConfirmationCounts();
	TestFalse(
		TEXT("Invalid structured damage is rejected"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			PlayerA.AbilitySystem,
			EnemyASC,
			InvalidRequest,
			PlayerA.Character));
	TestNoAdditionalPlayerConfirmation(TEXT("Invalid rejection"), InvalidBeforeCounts);

	FPRDamageRequest EnemySourceRequest;
	EnemySourceRequest.BaseDamage = 1.0f;
	EnemySourceRequest.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	EnemySourceRequest.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetAndSource;
	const FIntPoint EnemySourceBeforeCounts = CapturePlayerConfirmationCounts();
	TestTrue(
		TEXT("Enemy-source TargetAndSource damage against a player is accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemyASC,
			PlayerA.AbilitySystem,
			EnemySourceRequest,
			Enemy));
	TestNoAdditionalPlayerConfirmation(
		TEXT("Enemy source without a player owner"),
		EnemySourceBeforeCounts);

	return true;
}

#endif
