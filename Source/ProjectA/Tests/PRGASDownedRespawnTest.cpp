#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASDownedRespawnTest, "ProjectRift.GAS.DownedRespawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* FindProjectRiftClassForDownedRespawnTest(const TCHAR* ClassPath)
{
	return FindObject<UClass>(nullptr, ClassPath);
}

bool PossessTestCharacterForDownedRespawnTest(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
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

bool ApplyDamageForDownedRespawnTest(FAutomationTestBase& Test, APRPlayerState* TargetPlayerState, const float DamageAmount)
{
	UClass* DamageEffectClass = FindProjectRiftClassForDownedRespawnTest(TEXT("/Script/ProjectA.PRDamageGameplayEffect"));
	Test.TestNotNull(TEXT("GE_Damage class exists"), DamageEffectClass);
	if (!DamageEffectClass || !TargetPlayerState)
	{
		return false;
	}

	const FGameplayTag DamageTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage"), false);
	Test.TestTrue(TEXT("Data.Damage SetByCaller tag exists"), DamageTag.IsValid());
	if (!DamageTag.IsValid())
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

bool FPRGASDownedRespawnTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = APRCharacter::StaticClass();
	TestNotNull(TEXT("APRCharacter exposes downed state"), CharacterClass->FindFunctionByName(TEXT("IsDowned")));
	TestNotNull(TEXT("APRCharacter exposes alive state"), CharacterClass->FindFunctionByName(TEXT("IsAlive")));
	TestNotNull(TEXT("APRCharacter exposes manual downed entry"), CharacterClass->FindFunctionByName(TEXT("EnterDownedState")));
	TestNotNull(TEXT("APRCharacter exposes respawn entry"), CharacterClass->FindFunctionByName(TEXT("RespawnFromDowned")));
	TestNotNull(TEXT("APRCharacter exposes auto respawn timer state"), CharacterClass->FindFunctionByName(TEXT("IsAutoRespawnScheduled")));

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

	TestTrue(TEXT("Attacker initializes ASC"), PossessTestCharacterForDownedRespawnTest(World, AttackerPlayerState.Get(), Attacker.Get()));
	TestTrue(TEXT("Target initializes ASC"), PossessTestCharacterForDownedRespawnTest(World, TargetPlayerState.Get(), Target.Get()));

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

	const FGameplayTag DownedStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed"), false);
	TestTrue(TEXT("State.Downed tag is configured"), DownedStateTag.IsValid());

	AttackerAttributes->SetHealth(40.0f);
	AttackerAttributes->SetShield(0.0f);
	TestTrue(TEXT("Lethal damage applies to attacker"), ApplyDamageForDownedRespawnTest(*this, AttackerPlayerState.Get(), 250.0f));
	TestTrue(TEXT("Attacker enters downed state"), Attacker->IsDowned());
	TestFalse(TEXT("Downed attacker is not alive"), Attacker->IsAlive());
	TestTrue(TEXT("Downed state tag is present"), DownedStateTag.IsValid() && AttackerASC->HasMatchingGameplayTag(DownedStateTag));
	TestTrue(TEXT("Auto respawn is scheduled"), Attacker->IsAutoRespawnScheduled());
	TestEqual(TEXT("Downed movement is disabled"), Attacker->GetCharacterMovement()->MovementMode, MOVE_None);

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	Attacker->DoPrimaryAttack();
	TestEqual(TEXT("Downed attacker cannot damage shield"), TargetAttributes->GetShield(), 50.0f);
	TestEqual(TEXT("Downed attacker cannot damage health"), TargetAttributes->GetHealth(), 100.0f);

	TestTrue(TEXT("Manual respawn succeeds"), Attacker->RespawnFromDowned());
	TestFalse(TEXT("Respawn clears downed state"), Attacker->IsDowned());
	TestTrue(TEXT("Respawn restores alive state"), Attacker->IsAlive());
	TestFalse(TEXT("Respawn clears downed tag"), DownedStateTag.IsValid() && AttackerASC->HasMatchingGameplayTag(DownedStateTag));
	TestFalse(TEXT("Respawn clears auto timer"), Attacker->IsAutoRespawnScheduled());
	TestEqual(TEXT("Respawn restores health"), AttackerAttributes->GetHealth(), AttackerAttributes->GetMaxHealth());
	TestEqual(TEXT("Respawn restores shield"), AttackerAttributes->GetShield(), AttackerAttributes->GetMaxShield());
	TestEqual(TEXT("Respawn restores walking movement"), Attacker->GetCharacterMovement()->MovementMode, MOVE_Walking);

	Attacker->DoPrimaryAttack();
	TestEqual(TEXT("Respawned attacker can damage shield again"), TargetAttributes->GetShield(), 40.0f);
	TestEqual(TEXT("Respawned attacker still leaves health while shield absorbs"), TargetAttributes->GetHealth(), 100.0f);

	return true;
}

#endif
