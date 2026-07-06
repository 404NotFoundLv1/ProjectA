#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASPrimaryAttackAbilityTest, "ProjectRift.GAS.PrimaryAttackAbility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindProjectRiftClass(const TCHAR* ClassPath)
{
	return FindObject<UClass>(nullptr, ClassPath);
}

bool PossessTestCharacter(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
{
	if (!World || !PlayerState || !Character)
	{
		return false;
	}

	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	if (!PlayerController)
	{
		return false;
	}

	PlayerController->SetPlayerState(PlayerState);
	PlayerController->Possess(Character);
	return Character->IsAbilitySystemInitialized();
}
}

bool FPRGASPrimaryAttackAbilityTest::RunTest(const FString& Parameters)
{
	UClass* GameplayAbilityBaseClass = FindProjectRiftClass(TEXT("/Script/ProjectA.PRGameplayAbility"));
	UClass* PrimaryAttackAbilityClass = FindProjectRiftClass(TEXT("/Script/ProjectA.GA_PrimaryAttack"));
	UClass* DamageEffectClass = FindProjectRiftClass(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));

	TestNotNull(TEXT("UPRGameplayAbility class exists"), GameplayAbilityBaseClass);
	TestTrue(
		TEXT("UPRGameplayAbility derives from UGameplayAbility"),
		GameplayAbilityBaseClass && GameplayAbilityBaseClass->IsChildOf(UGameplayAbility::StaticClass()));
	TestNotNull(TEXT("UGA_PrimaryAttack class exists"), PrimaryAttackAbilityClass);
	TestTrue(
		TEXT("UGA_PrimaryAttack derives from UPRGameplayAbility"),
		PrimaryAttackAbilityClass && GameplayAbilityBaseClass && PrimaryAttackAbilityClass->IsChildOf(GameplayAbilityBaseClass));
	TestNotNull(TEXT("GE_Damage GameplayEffect exists"), DamageEffectClass);
	TestTrue(
		TEXT("GE_Damage derives from UGameplayEffect"),
		DamageEffectClass && DamageEffectClass->IsChildOf(UGameplayEffect::StaticClass()));

	UClass* CharacterClass = APRCharacter::StaticClass();
	TestNotNull(
		TEXT("APRCharacter exposes default ability grant entry point"),
		CharacterClass->FindFunctionByName(TEXT("GrantDefaultAbilities")));
	TestNotNull(
		TEXT("APRCharacter exposes default ability grant state"),
		CharacterClass->FindFunctionByName(TEXT("AreDefaultAbilitiesGranted")));

	if (!PrimaryAttackAbilityClass)
	{
		return false;
	}

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

	TStrongObjectPtr<APRPlayerState> AttackerPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> Attacker{ World->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator) };
	TStrongObjectPtr<APRPlayerState> TargetPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> Target{ World->SpawnActor<APRCharacter>(FVector(160.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };

	TestNotNull(TEXT("Attacker PlayerState spawned"), AttackerPlayerState.Get());
	TestNotNull(TEXT("Attacker Character spawned"), Attacker.Get());
	TestNotNull(TEXT("Target PlayerState spawned"), TargetPlayerState.Get());
	TestNotNull(TEXT("Target Character spawned"), Target.Get());
	if (!AttackerPlayerState || !Attacker || !TargetPlayerState || !Target)
	{
		return false;
	}

	TestTrue(TEXT("Attacker initializes ASC"), PossessTestCharacter(World, AttackerPlayerState.Get(), Attacker.Get()));
	TestTrue(TEXT("Target initializes ASC"), PossessTestCharacter(World, TargetPlayerState.Get(), Target.Get()));

	UPRAbilitySystemComponent* AttackerASC = Attacker->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Attacker has ASC"), AttackerASC);
	TestNotNull(TEXT("Target has AttributeSet"), TargetAttributes);
	if (!AttackerASC || !TargetAttributes)
	{
		return false;
	}

	FGameplayAbilitySpec* PrimaryAttackSpec = AttackerASC->FindAbilitySpecFromClass(PrimaryAttackAbilityClass);
	TestNotNull(TEXT("Attacker is granted primary attack ability"), PrimaryAttackSpec);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	Attacker->DoPrimaryAttack();
	TestEqual(TEXT("Primary attack lowers target shield first"), TargetAttributes->GetShield(), 40.0f);
	TestEqual(TEXT("Primary attack leaves health while shield absorbs damage"), TargetAttributes->GetHealth(), 100.0f);

	const FGameplayTag DeadStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Dead"), false);
	TestTrue(TEXT("Dead state tag is configured"), DeadStateTag.IsValid());
	if (DeadStateTag.IsValid())
	{
		AttackerASC->AddLooseGameplayTag(DeadStateTag);
		TargetAttributes->SetHealth(100.0f);
		TargetAttributes->SetShield(50.0f);
		Attacker->DoPrimaryAttack();
		TestEqual(TEXT("Primary attack cannot activate while dead"), TargetAttributes->GetHealth(), 100.0f);
		TestEqual(TEXT("Dead attacker does not damage shield"), TargetAttributes->GetShield(), 50.0f);
		AttackerASC->RemoveLooseGameplayTag(DeadStateTag);
	}

	return true;
}

#endif
