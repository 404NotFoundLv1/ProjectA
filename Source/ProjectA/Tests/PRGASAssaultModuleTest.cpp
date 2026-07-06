#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultBlast.h"
#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_AssaultShield.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRGameplayAbility.h"
#include "Abilities/PRTemporaryShieldGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASAssaultModuleTest, "ProjectRift.GAS.AssaultModule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool PossessTestCharacterForAssaultModuleTest(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
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

bool FPRGASAssaultModuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Assault charge derives from ProjectRift ability"), UGA_AssaultCharge::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Assault blast derives from ProjectRift ability"), UGA_AssaultBlast::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Assault shield derives from ProjectRift ability"), UGA_AssaultShield::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Temporary shield derives from GameplayEffect"), UPRTemporaryShieldGameplayEffect::StaticClass()->IsChildOf(UGameplayEffect::StaticClass()));

	UClass* CharacterClass = APRCharacter::StaticClass();
	TestNotNull(TEXT("APRCharacter exposes selected role grant entry point"), CharacterClass->FindFunctionByName(TEXT("GrantSelectedRoleModuleAbilities")));
	TestNotNull(TEXT("APRCharacter exposes role grant state"), CharacterClass->FindFunctionByName(TEXT("AreRoleModuleAbilitiesGranted")));

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
	TStrongObjectPtr<APRCharacter> Target{ World->SpawnActor<APRCharacter>(FVector(220.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };

	TestNotNull(TEXT("Attacker PlayerState spawned"), AttackerPlayerState.Get());
	TestNotNull(TEXT("Attacker Character spawned"), Attacker.Get());
	TestNotNull(TEXT("Target PlayerState spawned"), TargetPlayerState.Get());
	TestNotNull(TEXT("Target Character spawned"), Target.Get());
	if (!AttackerPlayerState || !Attacker || !TargetPlayerState || !Target)
	{
		return false;
	}

	AttackerPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Attacker initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, AttackerPlayerState.Get(), Attacker.Get()));
	TestTrue(TEXT("Target initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, TargetPlayerState.Get(), Target.Get()));

	UPRAbilitySystemComponent* AttackerASC = Attacker->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* AttackerAttributes = AttackerPlayerState->GetAttributeSet();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Attacker ASC exists"), AttackerASC);
	TestNotNull(TEXT("Attacker AttributeSet exists"), AttackerAttributes);
	TestNotNull(TEXT("Target AttributeSet exists"), TargetAttributes);
	if (!AttackerASC || !AttackerAttributes || !TargetAttributes)
	{
		return false;
	}

	TestTrue(TEXT("Assault role grants Q/E/R abilities during ASC initialization"), Attacker->AreRoleModuleAbilitiesGranted());
	TestNotNull(TEXT("Q charge ability is granted"), AttackerASC->FindAbilitySpecFromClass(UGA_AssaultCharge::StaticClass()));
	TestNotNull(TEXT("E blast ability is granted"), AttackerASC->FindAbilitySpecFromClass(UGA_AssaultBlast::StaticClass()));
	TestNotNull(TEXT("R shield ability is granted"), AttackerASC->FindAbilitySpecFromClass(UGA_AssaultShield::StaticClass()));

	AttackerAttributes->SetEnergy(100.0f);
	const FVector ChargeStart = Attacker->GetActorLocation();
	Attacker->DoSkillQ();
	TestTrue(TEXT("Q charge moves the attacker forward"), Attacker->GetActorLocation().X > ChargeStart.X + 250.0f);
	TestEqual(TEXT("Q charge consumes energy"), AttackerAttributes->GetEnergy(), 85.0f);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	Attacker->SetActorLocation(FVector::ZeroVector);
	Attacker->DoSkillE();
	TestEqual(TEXT("E blast damages target shield"), TargetAttributes->GetShield(), 25.0f);
	TestEqual(TEXT("E blast leaves health while shield absorbs"), TargetAttributes->GetHealth(), 100.0f);
	TestEqual(TEXT("E blast consumes energy"), AttackerAttributes->GetEnergy(), 60.0f);

	Attacker->DoSkillE();
	TestEqual(TEXT("E blast cooldown blocks immediate second damage"), TargetAttributes->GetShield(), 25.0f);
	TestEqual(TEXT("E blast cooldown does not consume energy again"), AttackerAttributes->GetEnergy(), 60.0f);

	AttackerAttributes->SetShield(10.0f);
	Attacker->DoSkillR();
	TestEqual(TEXT("R tactical shield grants shield"), AttackerAttributes->GetShield(), 40.0f);
	TestEqual(TEXT("R tactical shield consumes energy"), AttackerAttributes->GetEnergy(), 30.0f);

	TStrongObjectPtr<APRPlayerState> EnergyStarvedPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> EnergyStarvedAttacker{ World->SpawnActor<APRCharacter>(FVector(0.0f, 800.0f, 0.0f), FRotator::ZeroRotator) };
	if (!EnergyStarvedPlayerState || !EnergyStarvedAttacker)
	{
		return false;
	}

	EnergyStarvedPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Energy-starved attacker initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, EnergyStarvedPlayerState.Get(), EnergyStarvedAttacker.Get()));

	UPRAttributeSet* EnergyStarvedAttributes = EnergyStarvedPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Energy-starved AttributeSet exists"), EnergyStarvedAttributes);
	if (!EnergyStarvedAttributes)
	{
		return false;
	}

	EnergyStarvedAttributes->SetEnergy(0.0f);
	EnergyStarvedAttributes->SetShield(10.0f);
	EnergyStarvedAttacker->DoSkillR();
	TestEqual(TEXT("R tactical shield requires enough energy"), EnergyStarvedAttributes->GetShield(), 10.0f);

	TStrongObjectPtr<APRPlayerState> DownedPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> DownedAttacker{ World->SpawnActor<APRCharacter>(FVector(0.0f, 400.0f, 0.0f), FRotator::ZeroRotator) };
	TStrongObjectPtr<APRPlayerState> DownedTargetPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> DownedTarget{ World->SpawnActor<APRCharacter>(FVector(220.0f, 400.0f, 0.0f), FRotator::ZeroRotator) };
	if (!DownedPlayerState || !DownedAttacker || !DownedTargetPlayerState || !DownedTarget)
	{
		return false;
	}

	DownedPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Downed attacker initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, DownedPlayerState.Get(), DownedAttacker.Get()));
	TestTrue(TEXT("Downed target initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, DownedTargetPlayerState.Get(), DownedTarget.Get()));

	UPRAbilitySystemComponent* DownedASC = DownedAttacker->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* DownedTargetAttributes = DownedTargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Downed attacker ASC exists"), DownedASC);
	TestNotNull(TEXT("Downed target AttributeSet exists"), DownedTargetAttributes);
	if (!DownedASC || !DownedTargetAttributes)
	{
		return false;
	}

	const FGameplayTag DownedStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed"), false);
	TestTrue(TEXT("State.Downed tag is configured"), DownedStateTag.IsValid());
	if (DownedStateTag.IsValid())
	{
		DownedASC->AddLooseGameplayTag(DownedStateTag);
	}

	DownedTargetAttributes->SetShield(50.0f);
	DownedAttacker->DoSkillE();
	TestEqual(TEXT("Assault skills cannot activate while downed"), DownedTargetAttributes->GetShield(), 50.0f);

	return true;
}

#endif
