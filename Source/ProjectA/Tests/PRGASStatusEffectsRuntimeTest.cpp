#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRGASStatusEffectsRuntimeTest,
	"ProjectRift.GAS.StatusEffects.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool PossessStatusTestCharacter(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
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

void TickStatusTestWorld(UWorld* World, const float DurationSeconds)
{
	const float StepSeconds = 0.05f;
	for (float Elapsed = 0.0f; Elapsed < DurationSeconds; Elapsed += StepSeconds)
	{
		++GFrameCounter;
		World->Tick(ELevelTick::LEVELTICK_All, FMath::Min(StepSeconds, DurationSeconds - Elapsed));
	}
}

void TestStatusFloatNear(
	FAutomationTestBase& Test,
	const TCHAR* What,
	const float Actual,
	const float Expected,
	const float Tolerance = 0.02f)
{
	Test.TestTrue(What, FMath::IsNearlyEqual(Actual, Expected, Tolerance));
}

int32 CountNegativeStatusEffects(const UPRAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FActiveGameplayEffectHandle Handle : AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
		const UGameplayEffect* Definition = ActiveEffect ? ActiveEffect->Spec.Def : nullptr;
		if (Definition && (Definition->IsA(UPRPollutionStatusGameplayEffect::StaticClass())
			|| Definition->IsA(UPRSlowStatusGameplayEffect::StaticClass())
			|| Definition->IsA(UPRStunStatusGameplayEffect::StaticClass())))
		{
			++Count;
		}
	}
	return Count;
}
}

bool FPRGASStatusEffectsRuntimeTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Status test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Status test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	TStrongObjectPtr<APREnemyCharacter> EnemySource{ World->SpawnActor<APREnemyCharacter>(FVector(10000.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	TStrongObjectPtr<APRPlayerState> TargetPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> TargetPlayer{ World->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator) };
	TestNotNull(TEXT("Enemy source spawned"), EnemySource.Get());
	TestNotNull(TEXT("Player target state spawned"), TargetPlayerState.Get());
	TestNotNull(TEXT("Player target spawned"), TargetPlayer.Get());
	if (!EnemySource || !TargetPlayerState || !TargetPlayer)
	{
		return false;
	}

	TestTrue(TEXT("Player target initializes ASC"), PossessStatusTestCharacter(World, TargetPlayerState.Get(), TargetPlayer.Get()));
	UPRAbilitySystemComponent* EnemyASC = EnemySource->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* TargetASC = TargetPlayer->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Enemy ASC exists"), EnemyASC);
	TestNotNull(TEXT("Target ASC exists"), TargetASC);
	TestNotNull(TEXT("Target attributes exist"), TargetAttributes);
	if (!EnemyASC || !TargetASC || !TargetAttributes)
	{
		return false;
	}

	const FPRTargetStatusEffectDefinition PollutionDefinition(
		UPRPollutionStatusGameplayEffect::StaticClass(),
		2.0f,
		5.0f);
	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(0.0f);
	TargetAttributes->SetPollutionResistance(0.0f);
	const FActiveGameplayEffectHandle PollutionHandle = UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
		EnemyASC,
		TargetASC,
		PollutionDefinition,
		EnemySource.Get());
	TestTrue(TEXT("Pollution status applies"), PollutionHandle.IsValid());
	const FActiveGameplayEffect* ActivePollution = TargetASC->GetActiveGameplayEffect(PollutionHandle);
	TestNotNull(TEXT("Pollution remains active for periodic execution"), ActivePollution);
	if (ActivePollution)
	{
		TestEqual(
			TEXT("Periodic pollution preserves its original instigator"),
			ActivePollution->Spec.GetEffectContext().GetOriginalInstigator(),
			static_cast<AActor*>(EnemySource.Get()));
	}
	TestTrue(TEXT("Pollution grants its replicated state tag"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted));
	TickStatusTestWorld(World, 0.05f);
	TestStatusFloatNear(*this, TEXT("Pollution executes its first tick immediately"), TargetAttributes->GetHealth(), 97.8f);
	TickStatusTestWorld(World, 5.15f);
	TestStatusFloatNear(*this, TEXT("Pollution deals exactly five scaled ticks"), TargetAttributes->GetHealth(), 89.0f);
	TestFalse(TEXT("Pollution tag expires after five seconds"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted));

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetPollutionResistance(0.5f);
	const FPRTargetStatusEffectDefinition BriefPollutionDefinition(
		UPRPollutionStatusGameplayEffect::StaticClass(),
		2.0f,
		0.25f);
	TestTrue(
		TEXT("Brief pollution status applies"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemyASC,
			TargetASC,
			BriefPollutionDefinition,
			EnemySource.Get()).IsValid());
	TickStatusTestWorld(World, 0.05f);
	TestStatusFloatNear(*this, TEXT("Pollution resistance reduces periodic damage"), TargetAttributes->GetHealth(), 98.9f);
	TickStatusTestWorld(World, 0.25f);

	TargetAttributes->SetPollutionResistance(0.0f);
	TargetAttributes->SetMoveSpeed(600.0f);
	const FPRTargetStatusEffectDefinition SlowDefinition(
		UPRSlowStatusGameplayEffect::StaticClass(),
		0.70f,
		3.0f);
	TestTrue(
		TEXT("Slow status applies"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemyASC,
			TargetASC,
			SlowDefinition,
			EnemySource.Get()).IsValid());
	TestStatusFloatNear(*this, TEXT("Slow multiplies MoveSpeed"), TargetAttributes->GetMoveSpeed(), 420.0f);
	TestStatusFloatNear(*this, TEXT("Slow updates CharacterMovement"), TargetPlayer->GetCharacterMovement()->MaxWalkSpeed, 420.0f);
	TickStatusTestWorld(World, 3.05f);
	TestStatusFloatNear(*this, TEXT("MoveSpeed restores after slow expires"), TargetAttributes->GetMoveSpeed(), 600.0f);
	TestStatusFloatNear(*this, TEXT("CharacterMovement restores after slow expires"), TargetPlayer->GetCharacterMovement()->MaxWalkSpeed, 600.0f);

	const FPRTargetStatusEffectDefinition StunDefinition(
		UPRStunStatusGameplayEffect::StaticClass(),
		0.0f,
		1.25f);
	TestTrue(
		TEXT("Stun status applies"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemyASC,
			TargetASC,
			StunDefinition,
			EnemySource.Get()).IsValid());
	TestTrue(TEXT("Stun grants State.Stunned"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
	TestStatusFloatNear(*this, TEXT("Stun stops CharacterMovement"), TargetPlayer->GetCharacterMovement()->MaxWalkSpeed, 0.0f);
	TestFalse(
		TEXT("Stun blocks new primary attack activation"),
		TargetASC->TryActivateAbilityByClass(FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.GA_PrimaryAttack"))));
	TickStatusTestWorld(World, 1.3f);
	TestFalse(TEXT("State.Stunned expires"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
	TestStatusFloatNear(*this, TEXT("Movement restores when stun expires"), TargetPlayer->GetCharacterMovement()->MaxWalkSpeed, 600.0f);

	const FPRTargetStatusEffectDefinition FirstSlowDefinition(
		UPRSlowStatusGameplayEffect::StaticClass(),
		0.50f,
		1.0f);
	const FPRTargetStatusEffectDefinition ReplacementSlowDefinition(
		UPRSlowStatusGameplayEffect::StaticClass(),
		0.80f,
		2.0f);
	UPRCombatEffectLibrary::ApplyStatusEffectToTarget(EnemyASC, TargetASC, FirstSlowDefinition, EnemySource.Get());
	TickStatusTestWorld(World, 0.5f);
	UPRCombatEffectLibrary::ApplyStatusEffectToTarget(EnemyASC, TargetASC, ReplacementSlowDefinition, EnemySource.Get());
	TestEqual(TEXT("Replacing a status keeps exactly one active negative status instance"), CountNegativeStatusEffects(TargetASC), 1);
	TestStatusFloatNear(*this, TEXT("Replacement status uses latest magnitude"), TargetAttributes->GetMoveSpeed(), 480.0f);
	TickStatusTestWorld(World, 0.7f);
	TestTrue(TEXT("Replacement status restarts its duration"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));
	TickStatusTestWorld(World, 1.35f);
	TestFalse(TEXT("Replacement status eventually expires"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));

	UPRCombatEffectLibrary::ApplyStatusEffectToTarget(EnemyASC, TargetASC, SlowDefinition, EnemySource.Get());
	UPRCombatEffectLibrary::ApplyStatusEffectToTarget(EnemyASC, TargetASC, StunDefinition, EnemySource.Get());
	TestTrue(TEXT("Target has negative states before downing"), CountNegativeStatusEffects(TargetASC) >= 2);
	TargetAttributes->SetHealth(1.0f);
	TargetAttributes->SetShield(0.0f);
	TestTrue(
		TEXT("Lethal pollution status applies"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemyASC,
			TargetASC,
			PollutionDefinition,
			EnemySource.Get()).IsValid());
	TickStatusTestWorld(World, 0.05f);
	TestTrue(TEXT("Periodic pollution can enter the target into downed state"), TargetPlayer->IsDowned());
	TestFalse(TEXT("Downing clears slow"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));
	TestFalse(TEXT("Downing clears stun"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
	TestFalse(TEXT("Downing clears pollution"), TargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Polluted));
	TestEqual(TEXT("Downing removes active negative status effects without removing persistent role energy regeneration"), CountNegativeStatusEffects(TargetASC), 0);
	TestFalse(
		TEXT("Downed targets reject damage"),
		UPRCombatEffectLibrary::ApplyDamageToTarget(
			EnemyASC,
			TargetASC,
			10.0f,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			EnemySource.Get()));
	TestFalse(
		TEXT("Downed targets reject status effects"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemyASC,
			TargetASC,
			SlowDefinition,
			EnemySource.Get()).IsValid());

	TStrongObjectPtr<APRPlayerState> FriendlySourceState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> FriendlySource{ World->SpawnActor<APRCharacter>(FVector(20000.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	TStrongObjectPtr<APRPlayerState> FriendlyTargetState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> FriendlyTarget{ World->SpawnActor<APRCharacter>(FVector(20100.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	if (!FriendlySourceState || !FriendlySource || !FriendlyTargetState || !FriendlyTarget)
	{
		return false;
	}
	TestTrue(TEXT("Friendly source initializes ASC"), PossessStatusTestCharacter(World, FriendlySourceState.Get(), FriendlySource.Get()));
	TestTrue(TEXT("Friendly target initializes ASC"), PossessStatusTestCharacter(World, FriendlyTargetState.Get(), FriendlyTarget.Get()));
	UPRAbilitySystemComponent* FriendlySourceASC = FriendlySource->GetProjectRiftAbilitySystemComponent();
	UPRAbilitySystemComponent* FriendlyTargetASC = FriendlyTarget->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* FriendlyTargetAttributes = FriendlyTargetState->GetAttributeSet();
	FriendlyTargetAttributes->SetHealth(100.0f);
	FriendlyTargetAttributes->SetShield(50.0f);
	TestFalse(TEXT("Players are not hostile to other players"), UPRCombatEffectLibrary::IsHostileTarget(FriendlySourceASC, FriendlyTargetASC));
	TestFalse(
		TEXT("Player friendly damage is rejected"),
		UPRCombatEffectLibrary::ApplyDamageToTarget(
			FriendlySourceASC,
			FriendlyTargetASC,
			20.0f,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			FriendlySource.Get()));
	TestFalse(
		TEXT("Player friendly status is rejected"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			FriendlySourceASC,
			FriendlyTargetASC,
			SlowDefinition,
			FriendlySource.Get()).IsValid());
	TestStatusFloatNear(*this, TEXT("Friendly-fire rejection preserves shield"), FriendlyTargetAttributes->GetShield(), 50.0f);
	TestStatusFloatNear(*this, TEXT("Friendly-fire rejection preserves MoveSpeed"), FriendlyTargetAttributes->GetMoveSpeed(), 600.0f);

	return true;
}

#endif
