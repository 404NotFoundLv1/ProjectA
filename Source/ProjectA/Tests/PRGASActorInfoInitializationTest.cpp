#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASActorInfoInitializationTest, "ProjectRift.GAS.ActorInfoInitialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRGASActorInfoInitializationTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = APRCharacter::StaticClass();

	TestNotNull(
		TEXT("APRCharacter exposes ASC getter for input and UI"),
		CharacterClass->FindFunctionByName(TEXT("GetProjectRiftAbilitySystemComponent")));
	TestNotNull(
		TEXT("APRCharacter exposes AttributeSet getter for UI"),
		CharacterClass->FindFunctionByName(TEXT("GetAttributeSet")));
	TestNotNull(
		TEXT("APRCharacter exposes initialization state"),
		CharacterClass->FindFunctionByName(TEXT("IsAbilitySystemInitialized")));
	TestNotNull(
		TEXT("APRCharacter exposes ASC initialization entry point"),
		CharacterClass->FindFunctionByName(TEXT("InitializeAbilitySystemFromPlayerState")));

	const FObjectPropertyBase* CachedASCProperty = FindFProperty<FObjectPropertyBase>(CharacterClass, TEXT("AbilitySystemComponent"));
	TestNotNull(TEXT("APRCharacter caches the player ASC"), CachedASCProperty);
	TestTrue(
		TEXT("Cached ASC property is a UAbilitySystemComponent"),
		CachedASCProperty && CachedASCProperty->PropertyClass->IsChildOf(UAbilitySystemComponent::StaticClass()));

	const FMulticastInlineDelegateProperty* InitializedDelegate = FindFProperty<FMulticastInlineDelegateProperty>(CharacterClass, TEXT("OnAbilitySystemInitialized"));
	TestNotNull(TEXT("APRCharacter broadcasts ASC initialization"), InitializedDelegate);
	TestTrue(
		TEXT("ASC initialization delegate is assignable from Blueprint"),
		InitializedDelegate && InitializedDelegate->HasAnyPropertyFlags(CPF_BlueprintAssignable));

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

	TStrongObjectPtr<APRPlayerState> PlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> Character{ World->SpawnActor<APRCharacter>() };
	TStrongObjectPtr<APlayerController> PlayerController{ World->SpawnActor<APlayerController>() };
	TestNotNull(TEXT("Test PlayerState spawned"), PlayerState.Get());
	TestNotNull(TEXT("Test Character spawned"), Character.Get());
	TestNotNull(TEXT("Test PlayerController spawned"), PlayerController.Get());
	if (!PlayerState || !Character || !PlayerController)
	{
		return false;
	}

	TestNotNull(TEXT("Test PlayerState owns ASC"), PlayerState->GetAbilitySystemComponent());
	TestFalse(TEXT("Character starts without initialized ASC"), Character->IsAbilitySystemInitialized());

	PlayerController->SetPlayerState(PlayerState.Get());
	PlayerController->Possess(Character.Get());

	TestTrue(TEXT("Character initializes ASC when possessed"), Character->IsAbilitySystemInitialized());
	TestTrue(TEXT("Character reports initialized ASC"), Character->IsAbilitySystemInitialized());

	UPRAbilitySystemComponent* CharacterASC = Character->GetProjectRiftAbilitySystemComponent();
	TestEqual(
		TEXT("Character caches PlayerState ASC"),
		CharacterASC,
		PlayerState->GetProjectRiftAbilitySystemComponent());
	TestEqual(
		TEXT("ASC owner actor remains PlayerState"),
		CharacterASC->GetOwnerActor(),
		static_cast<AActor*>(PlayerState.Get()));
	TestEqual(
		TEXT("ASC avatar actor becomes Character"),
		CharacterASC->GetAvatarActor(),
		static_cast<AActor*>(Character.Get()));

	return true;
}

#endif
