#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Revive/PRReviveComponent.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRReviveRuntimeTest, "ProjectRift.Revive.Runtime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool PossessReviveTestCharacter(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
{
	APlayerController* Controller = World ? World->SpawnActor<APlayerController>() : nullptr;
	if (!Controller || !PlayerState || !Character)
	{
		return false;
	}
	Controller->SetPlayerState(PlayerState);
	Controller->Possess(Character);
	return Character->IsAbilitySystemInitialized() && Character->GetReviveComponent();
}

void AdvanceReviveTestWorld(UWorld* World, const float Duration)
{
	for (float Elapsed = 0.0f; World && Elapsed < Duration; Elapsed += 0.1f)
	{
		++GFrameCounter;
		World->Tick(ELevelTick::LEVELTICK_All, FMath::Min(0.1f, Duration - Elapsed));
	}
}
}

bool FPRReviveRuntimeTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Revive test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	TestTrue(TEXT("Revive test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	TStrongObjectPtr<APRPlayerState> RescuerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRPlayerState> TargetState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> Rescuer{ World->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator) };
	TStrongObjectPtr<APRCharacter> Target{ World->SpawnActor<APRCharacter>(FVector(120.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	TestTrue(TEXT("Rescuer initializes for revive"), PossessReviveTestCharacter(World, RescuerState.Get(), Rescuer.Get()));
	TestTrue(TEXT("Target initializes for revive"), PossessReviveTestCharacter(World, TargetState.Get(), Target.Get()));
	if (!Rescuer || !Target || !TargetState)
	{
		return false;
	}

	UPRAttributeSet* TargetAttributes = TargetState->GetAttributeSet();
	UPRReviveComponent* TargetRevive = Target->GetReviveComponent();
	UPRAbilitySystemComponent* RescuerASC = Rescuer->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* TargetASC = Target->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("Target attributes exist"), TargetAttributes);
	TestNotNull(TEXT("Target revive component exists"), TargetRevive);
	if (!TargetAttributes || !TargetRevive || !RescuerASC || !TargetASC)
	{
		return false;
	}

	TestTrue(TEXT("Target enters server-authoritative downed state"), Target->EnterDownedState());
	TestFalse(TEXT("Downed state no longer schedules auto respawn"), Target->IsAutoRespawnScheduled());
	TestTrue(TEXT("Rescuer starts a three-second revive"), TargetRevive->BeginPlayerRevive(Rescuer.Get()));
	TestTrue(TEXT("Rescuer receives State.Reviving"), RescuerASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Reviving));
	TestTrue(TEXT("Target receives State.BeingRevived"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_BeingRevived));
	AdvanceReviveTestWorld(World, 3.2f);
	TestFalse(TEXT("Completed revive clears downed state"), Target->IsDowned());
	TestTrue(TEXT("Completed revive restores life"), Target->IsAlive());
	TestTrue(TEXT("Completed revive restores forty percent health"), FMath::IsNearlyEqual(TargetAttributes->GetHealth(), TargetAttributes->GetMaxHealth() * 0.4f));
	TestEqual(TEXT("Completed revive restores zero shield"), TargetAttributes->GetShield(), 0.0f);
	TestFalse(TEXT("Completed revive clears rescuer tag"), RescuerASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Reviving));

	TestTrue(TEXT("Target can enter downed state again before bleed-out"), Target->EnterDownedState());
	TestTrue(TEXT("Movement cancels an occupied revive"), TargetRevive->BeginPlayerRevive(Rescuer.Get()));
	Rescuer->SetActorLocation(FVector(250.0f, 0.0f, 0.0f));
	AdvanceReviveTestWorld(World, 0.2f);
	TestFalse(TEXT("Moving rescuer clears revive progress"), TargetRevive->IsReviveInProgress());
	Rescuer->SetActorLocation(FVector::ZeroVector);
	AdvanceReviveTestWorld(World, 30.2f);
	TestTrue(TEXT("Thirty-second bleed-out adds dead state"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead));
	TestFalse(TEXT("Bleed-out target cannot be revived"), TargetRevive->BeginPlayerRevive(Rescuer.Get()));

	return true;
}

#endif
