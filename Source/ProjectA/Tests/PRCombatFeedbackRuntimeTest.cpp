#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffectTypes.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCombatFeedbackRuntimeTest,
	"ProjectRift.GAS.CombatFeedback.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FCombatFeedbackTestPlayer
{
	APRPlayerState* PlayerState = nullptr;
	APRCharacter* Character = nullptr;
	APlayerController* Controller = nullptr;
	UPRAbilitySystemComponent* AbilitySystem = nullptr;
	UPRAttributeSet* Attributes = nullptr;
	UPRCombatFeedbackComponent* Feedback = nullptr;
};

struct FObservedCombatFeedback
{
	bool bHasResolvedDamage = false;
	bool bHasEffectContext = false;
	bool bHasDispatchCount = false;
	FPRResolvedDamage ResolvedDamage;
	FGameplayEffectContextHandle EffectContext;
	int32 FeedbackDispatchCount = 0;
};

FCombatFeedbackTestPlayer SpawnCombatFeedbackTestPlayer(
	UWorld* World,
	const FVector& Location)
{
	FCombatFeedbackTestPlayer Result;
	if (!World)
	{
		return Result;
	}

	Result.PlayerState = World->SpawnActor<APRPlayerState>();
	Result.Character = World->SpawnActor<APRCharacter>(Location, FRotator::ZeroRotator);
	Result.Controller = World->SpawnActor<APlayerController>();
	if (Result.PlayerState && Result.Character && Result.Controller)
	{
		Result.Controller->SetPlayerState(Result.PlayerState);
		Result.Controller->Possess(Result.Character);
		Result.AbilitySystem = Result.Character->GetProjectRiftAbilitySystemComponent();
		Result.Attributes = Result.PlayerState->GetAttributeSet();
		Result.Feedback = Result.Character->FindComponentByClass<UPRCombatFeedbackComponent>();
	}
	return Result;
}

void TickCombatFeedbackWorld(UWorld* World, const float DurationSeconds)
{
	const float StepSeconds = 0.02f;
	for (float Elapsed = 0.0f; World && Elapsed < DurationSeconds; Elapsed += StepSeconds)
	{
		++GFrameCounter;
		World->Tick(
			ELevelTick::LEVELTICK_All,
			FMath::Min(StepSeconds, DurationSeconds - Elapsed));
	}
}

void TestCombatFeedbackFloatNear(
	FAutomationTestBase& Test,
	const TCHAR* What,
	const float Actual,
	const float Expected,
	const float Tolerance = 0.02f)
{
	Test.TestTrue(What, FMath::IsNearlyEqual(Actual, Expected, Tolerance));
}

FPRDamageRequest MakeDamageRequest(
	AActor* Target,
	const float BaseDamage,
	const EPRHitReactionStrength Strength,
	const float DurationSeconds,
	const FVector& ImpactPoint = FVector(42.0f, 17.0f, 93.0f),
	const FVector& ImpactNormal = FVector(-1.0f, 0.0f, 0.0f))
{
	FPRDamageRequest Request;
	Request.BaseDamage = BaseDamage;
	Request.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	Request.HitReaction.Strength = Strength;
	Request.HitReaction.DurationSeconds = DurationSeconds;
	Request.FeedbackPolicy = EPRCombatFeedbackPolicy::TargetAndSource;
	Request.HitResult = FHitResult(
		Target,
		Target ? Target->FindComponentByClass<UCapsuleComponent>() : nullptr,
		ImpactPoint,
		ImpactNormal);
	Request.HitResult.bBlockingHit = true;
	Request.HitResult.TraceStart = ImpactPoint - ImpactNormal * 100.0f;
	Request.HitResult.TraceEnd = ImpactPoint;
	return Request;
}

FObservedCombatFeedback ObserveCombatFeedback(
	FAutomationTestBase& Test,
	UPRCombatFeedbackComponent* Feedback,
	const TCHAR* OwnerLabel)
{
	FObservedCombatFeedback Observation;
	Test.TestNotNull(FString::Printf(TEXT("%s owns a combat feedback component"), OwnerLabel), Feedback);
	if (!Feedback)
	{
		return Observation;
	}

	FStructProperty* ResolvedDamageProperty = FindFProperty<FStructProperty>(
		Feedback->GetClass(),
		TEXT("LastResolvedDamage"));
	Test.TestNotNull(
		FString::Printf(TEXT("%s feedback exposes transient LastResolvedDamage for runtime observation"), OwnerLabel),
		ResolvedDamageProperty);
	if (ResolvedDamageProperty)
	{
		Test.TestEqual<const UScriptStruct*>(
			FString::Printf(TEXT("%s LastResolvedDamage uses FPRResolvedDamage"), OwnerLabel),
			ResolvedDamageProperty->Struct.Get(),
			FPRResolvedDamage::StaticStruct());
		Test.TestTrue(
			FString::Printf(TEXT("%s LastResolvedDamage remains transient"), OwnerLabel),
			ResolvedDamageProperty->HasAnyPropertyFlags(CPF_Transient));
		if (ResolvedDamageProperty->Struct == FPRResolvedDamage::StaticStruct())
		{
			Observation.ResolvedDamage = *ResolvedDamageProperty->ContainerPtrToValuePtr<FPRResolvedDamage>(Feedback);
			Observation.bHasResolvedDamage = true;
		}
	}

	FStructProperty* EffectContextProperty = FindFProperty<FStructProperty>(
		Feedback->GetClass(),
		TEXT("LastDamageEffectContext"));
	Test.TestNotNull(
		FString::Printf(TEXT("%s feedback exposes transient LastDamageEffectContext for runtime observation"), OwnerLabel),
		EffectContextProperty);
	if (EffectContextProperty)
	{
		Test.TestEqual<const UScriptStruct*>(
			FString::Printf(TEXT("%s LastDamageEffectContext uses FGameplayEffectContextHandle"), OwnerLabel),
			EffectContextProperty->Struct.Get(),
			FGameplayEffectContextHandle::StaticStruct());
		Test.TestTrue(
			FString::Printf(TEXT("%s LastDamageEffectContext remains transient"), OwnerLabel),
			EffectContextProperty->HasAnyPropertyFlags(CPF_Transient));
		if (EffectContextProperty->Struct == FGameplayEffectContextHandle::StaticStruct())
		{
			Observation.EffectContext = *EffectContextProperty->ContainerPtrToValuePtr<FGameplayEffectContextHandle>(Feedback);
			Observation.bHasEffectContext = true;
		}
	}

	FIntProperty* DispatchCountProperty = FindFProperty<FIntProperty>(
		Feedback->GetClass(),
		TEXT("FeedbackDispatchCount"));
	Test.TestNotNull(
		FString::Printf(TEXT("%s feedback exposes transient FeedbackDispatchCount for dispatch observation"), OwnerLabel),
		DispatchCountProperty);
	if (DispatchCountProperty)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s FeedbackDispatchCount remains transient"), OwnerLabel),
			DispatchCountProperty->HasAnyPropertyFlags(CPF_Transient));
		Observation.FeedbackDispatchCount = DispatchCountProperty->GetPropertyValue_InContainer(Feedback);
		Observation.bHasDispatchCount = true;
	}

	return Observation;
}

void ResetCombatFeedbackObservation(UPRCombatFeedbackComponent* Feedback)
{
	if (!Feedback)
	{
		return;
	}

	if (FStructProperty* ResolvedDamageProperty = FindFProperty<FStructProperty>(
		Feedback->GetClass(),
		TEXT("LastResolvedDamage"));
		ResolvedDamageProperty && ResolvedDamageProperty->Struct == FPRResolvedDamage::StaticStruct())
	{
		*ResolvedDamageProperty->ContainerPtrToValuePtr<FPRResolvedDamage>(Feedback) = FPRResolvedDamage();
	}
	if (FStructProperty* EffectContextProperty = FindFProperty<FStructProperty>(
		Feedback->GetClass(),
		TEXT("LastDamageEffectContext"));
		EffectContextProperty && EffectContextProperty->Struct == FGameplayEffectContextHandle::StaticStruct())
	{
		*EffectContextProperty->ContainerPtrToValuePtr<FGameplayEffectContextHandle>(Feedback) = FGameplayEffectContextHandle();
	}
	if (FIntProperty* DispatchCountProperty = FindFProperty<FIntProperty>(
		Feedback->GetClass(),
		TEXT("FeedbackDispatchCount")))
	{
		DispatchCountProperty->SetPropertyValue_InContainer(Feedback, 0);
	}
}
}

bool FPRCombatFeedbackRuntimeTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Combat feedback test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Combat feedback test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APREnemyCharacter* EnemySource = World->SpawnActor<APREnemyCharacter>(
		FVector(10000.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	FCombatFeedbackTestPlayer ResolutionTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector::ZeroVector);
	TestNotNull(TEXT("Enemy damage source spawns"), EnemySource);
	TestTrue(
		TEXT("Resolution target initializes real GAS actor info"),
		ResolutionTarget.Character && ResolutionTarget.Character->IsAbilitySystemInitialized());
	if (!EnemySource
		|| !ResolutionTarget.AbilitySystem
		|| !ResolutionTarget.Attributes
		|| !ResolutionTarget.Feedback)
	{
		return false;
	}

	UPRAbilitySystemComponent* EnemySourceASC = EnemySource->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("Enemy damage source owns its real ASC"), EnemySourceASC);
	if (!EnemySourceASC)
	{
		return false;
	}
	const UPRAttributeSet* EnemySourceAttributes = EnemySourceASC->GetSet<UPRAttributeSet>();
	TestEqual(TEXT("Enemy ASC owner is the enemy actor"), EnemySourceASC->GetOwnerActor(), static_cast<AActor*>(EnemySource));
	TestEqual(TEXT("Enemy ASC avatar is the enemy actor"), EnemySourceASC->GetAvatarActor(), static_cast<AActor*>(EnemySource));
	TestEqual(TEXT("Player ASC owner is its PlayerState"), ResolutionTarget.AbilitySystem->GetOwnerActor(), static_cast<AActor*>(ResolutionTarget.PlayerState));
	TestEqual(TEXT("Player ASC avatar is its possessed character"), ResolutionTarget.AbilitySystem->GetAvatarActor(), static_cast<AActor*>(ResolutionTarget.Character));
	TestNotNull(TEXT("Enemy ASC exposes the shared AttributeSet"), EnemySourceAttributes);
	if (EnemySourceAttributes)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Enemy source AttackPower is 10 for damage expectations"), EnemySourceAttributes->GetAttackPower(), 10.0f);
	}

	// Structured damage must preserve its trace context and report post-absorption damage.
	ResolutionTarget.Attributes->SetHealth(100.0f);
	ResolutionTarget.Attributes->SetShield(10.0f);
	ResetCombatFeedbackObservation(ResolutionTarget.Feedback);
	const FVector ExpectedImpactPoint(42.0f, 17.0f, 93.0f);
	const FVector ExpectedImpactNormal(-1.0f, 0.0f, 0.0f);
	const FPRDamageRequest SplitDamageRequest = MakeDamageRequest(
		ResolutionTarget.Character,
		20.0f,
		EPRHitReactionStrength::None,
		0.0f,
		ExpectedImpactPoint,
		ExpectedImpactNormal);
	TestTrue(
		TEXT("Structured physical damage is accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			ResolutionTarget.AbilitySystem,
			SplitDamageRequest,
			EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("Structured damage consumes shield first"), ResolutionTarget.Attributes->GetShield(), 0.0f);
	TestCombatFeedbackFloatNear(*this, TEXT("Structured damage overflows into health"), ResolutionTarget.Attributes->GetHealth(), 88.0f);

	const FObservedCombatFeedback SplitObservation = ObserveCombatFeedback(
		*this,
		ResolutionTarget.Feedback,
		TEXT("Resolution target"));
	if (SplitObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("One accepted structured hit dispatches feedback exactly once"), SplitObservation.FeedbackDispatchCount, 1);
	}
	if (SplitObservation.bHasResolvedDamage)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Feedback reports actual shield damage"), SplitObservation.ResolvedDamage.ShieldDamage, 10.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Feedback reports actual health damage"), SplitObservation.ResolvedDamage.HealthDamage, 12.0f);
		TestTrue(TEXT("Feedback reports shield break"), SplitObservation.ResolvedDamage.bShieldBroken);
		TestFalse(TEXT("Nonlethal split damage is not reported lethal"), SplitObservation.ResolvedDamage.bLethal);
	}
	if (SplitObservation.bHasEffectContext)
	{
		TestEqual(
			TEXT("Structured damage preserves original instigator"),
			SplitObservation.EffectContext.GetOriginalInstigator(),
			static_cast<AActor*>(EnemySource));
		const FHitResult* ObservedHitResult = SplitObservation.EffectContext.GetHitResult();
		TestNotNull(TEXT("Structured damage stores its FHitResult in EffectContext"), ObservedHitResult);
		if (ObservedHitResult)
		{
			TestTrue(TEXT("EffectContext preserves impact point"), ObservedHitResult->ImpactPoint.Equals(ExpectedImpactPoint, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("EffectContext preserves impact normal"), ObservedHitResult->ImpactNormal.Equals(ExpectedImpactNormal, KINDA_SMALL_NUMBER));
			TestEqual(TEXT("EffectContext preserves hit target"), ObservedHitResult->GetActor(), static_cast<AActor*>(ResolutionTarget.Character));
		}
	}

	FPRDamageRequest PreLethalHeavyRequest = MakeDamageRequest(
		ResolutionTarget.Character,
		1.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestTrue(
		TEXT("Player accepts a heavy hit before lethal damage"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			ResolutionTarget.AbilitySystem,
			PreLethalHeavyRequest,
			EnemySource));
	TestTrue(
		TEXT("Player is hit-staggered before lethal damage"),
		ResolutionTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	ResolutionTarget.Attributes->SetHealth(5.0f);
	ResolutionTarget.Attributes->SetShield(5.0f);
	ResetCombatFeedbackObservation(ResolutionTarget.Feedback);
	const FPRDamageRequest LethalResolutionRequest = MakeDamageRequest(
		ResolutionTarget.Character,
		20.0f,
		EPRHitReactionStrength::Heavy,
		0.30f);
	TestTrue(
		TEXT("Lethal structured damage is accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			ResolutionTarget.AbilitySystem,
			LethalResolutionRequest,
			EnemySource));
	const FObservedCombatFeedback LethalObservation = ObserveCombatFeedback(
		*this,
		ResolutionTarget.Feedback,
		TEXT("Lethal resolution target"));
	if (LethalObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Lethal structured hit dispatches feedback exactly once"), LethalObservation.FeedbackDispatchCount, 1);
	}
	if (LethalObservation.bHasResolvedDamage)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Lethal feedback reports only remaining shield"), LethalObservation.ResolvedDamage.ShieldDamage, 5.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Lethal feedback reports only remaining health"), LethalObservation.ResolvedDamage.HealthDamage, 5.0f);
		TestTrue(TEXT("Lethal feedback reports shield break"), LethalObservation.ResolvedDamage.bShieldBroken);
		TestTrue(TEXT("Lethal feedback reports lethal result"), LethalObservation.ResolvedDamage.bLethal);
	}
	TestFalse(
		TEXT("Entering downed state clears prior stagger and lethal hit never re-adds it"),
		ResolutionTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestTrue(TEXT("Lethal player damage enters the authoritative downed state"), ResolutionTarget.Character->IsDowned());

	// Use a fresh living target for stagger priority, refresh, movement and cleanup behavior.
	FCombatFeedbackTestPlayer StaggerTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(0.0f, 1000.0f, 0.0f));
	TestTrue(
		TEXT("Stagger target initializes real GAS actor info"),
		StaggerTarget.Character && StaggerTarget.Character->IsAbilitySystemInitialized());
	if (!StaggerTarget.AbilitySystem || !StaggerTarget.Attributes || !StaggerTarget.Character)
	{
		return false;
	}
	StaggerTarget.Attributes->SetHealth(100.0f);
	StaggerTarget.Attributes->SetShield(50.0f);
	StaggerTarget.Attributes->SetMoveSpeed(487.0f);

	const FPRDamageRequest LightRequest = MakeDamageRequest(
		StaggerTarget.Character,
		1.0f,
		EPRHitReactionStrength::Light,
		0.12f);
	TestTrue(TEXT("Light stagger damage is accepted"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LightRequest, EnemySource));
	TestTrue(TEXT("Light hit grants State.HitStaggered"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestEqual(TEXT("Feedback component tracks active Light strength"), StaggerTarget.Feedback->GetActiveStaggerStrength(), EPRHitReactionStrength::Light);
	TestTrue(TEXT("Feedback component tracks a valid active Light handle"), StaggerTarget.Feedback->GetActiveStaggerHandle().IsValid());
	TestCombatFeedbackFloatNear(*this, TEXT("Hit stagger stops CharacterMovement"), StaggerTarget.Character->GetCharacterMovement()->MaxWalkSpeed, 0.0f);
	TickCombatFeedbackWorld(World, 0.14f);
	TestFalse(TEXT("Light stagger expires at its configured duration"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestEqual(TEXT("Natural stagger expiry resets tracked strength to None"), StaggerTarget.Feedback->GetActiveStaggerStrength(), EPRHitReactionStrength::None);
	TestFalse(TEXT("Natural stagger expiry invalidates the tracked handle"), StaggerTarget.Feedback->GetActiveStaggerHandle().IsValid());
	TestCombatFeedbackFloatNear(*this, TEXT("Movement restores the current aggregated 487 speed after hit stagger"), StaggerTarget.Character->GetCharacterMovement()->MaxWalkSpeed, 487.0f);

	FCombatFeedbackTestPlayer NoFeedbackTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(1400.0f, 800.0f, 0.0f));
	TestNotNull(TEXT("No-feedback stagger target initially owns the standard feedback component"), NoFeedbackTarget.Feedback);
	if (!NoFeedbackTarget.AbilitySystem || !NoFeedbackTarget.Attributes || !NoFeedbackTarget.Feedback)
	{
		return false;
	}
	TWeakObjectPtr<UPRCombatFeedbackComponent> RemovedFeedback = NoFeedbackTarget.Feedback;
	NoFeedbackTarget.Feedback->DestroyComponent();
	NoFeedbackTarget.Feedback = nullptr;
	TestNull(
		TEXT("No-feedback stagger target no longer exposes a feedback component"),
		NoFeedbackTarget.Character->FindComponentByClass<UPRCombatFeedbackComponent>());
	NoFeedbackTarget.Attributes->SetHealth(100.0f);
	NoFeedbackTarget.Attributes->SetShield(50.0f);
	const FPRDamageRequest NoComponentLightRequest = MakeDamageRequest(
		NoFeedbackTarget.Character,
		1.0f,
		EPRHitReactionStrength::Light,
		0.12f);
	TestTrue(
		TEXT("Structured Light damage remains accepted without a presentation component"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			NoFeedbackTarget.AbilitySystem,
			NoComponentLightRequest,
			EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("No-component target still settles structured damage"), NoFeedbackTarget.Attributes->GetShield(), 48.9f);
	TestTrue(
		TEXT("Missing feedback presentation never suppresses State.HitStaggered"),
		NoFeedbackTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestNull(
		TEXT("Structured damage does not recreate a missing feedback component"),
		NoFeedbackTarget.Character->FindComponentByClass<UPRCombatFeedbackComponent>());
	if (RemovedFeedback.IsValid())
	{
		TestEqual(TEXT("Removed feedback component records no resolved feedback"), RemovedFeedback->FeedbackDispatchCount, 0);
	}
	TickCombatFeedbackWorld(World, 0.14f);
	TestFalse(
		TEXT("No-component Light stagger still expires naturally"),
		NoFeedbackTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	FPRDamageRequest RefreshingLightRequest = LightRequest;
	RefreshingLightRequest.HitReaction.DurationSeconds = 0.20f;
	TestTrue(
		TEXT("First refreshable Light hit is accepted at t=0.00s"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, RefreshingLightRequest, EnemySource));
	TickCombatFeedbackWorld(World, 0.12f);
	TestTrue(
		TEXT("Second same-strength Light hit is accepted at t=0.12s"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, RefreshingLightRequest, EnemySource));
	TickCombatFeedbackWorld(World, 0.12f);
	TestTrue(TEXT("Refreshed 0.20s Light stagger remains at t=0.24s"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TickCombatFeedbackWorld(World, 0.10f);
	TestFalse(TEXT("Refreshed 0.20s Light stagger expires by t=0.34s"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	FPRDamageRequest LongLightRequest = LightRequest;
	LongLightRequest.HitReaction.DurationSeconds = 1.0f;
	FPRDamageRequest ShortHeavyRequest = MakeDamageRequest(
		StaggerTarget.Character,
		1.0f,
		EPRHitReactionStrength::Heavy,
		0.20f);
	TestTrue(
		TEXT("Long Light hit is accepted before Heavy replacement"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LongLightRequest, EnemySource));
	TickCombatFeedbackWorld(World, 0.02f);
	TestTrue(
		TEXT("Short Heavy hit is accepted at t=0.02s"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, ShortHeavyRequest, EnemySource));
	TestTrue(TEXT("Short Heavy replacement is active immediately at t=0.02s"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TickCombatFeedbackWorld(World, 0.22f);
	TestFalse(TEXT("Heavy replacement removes weaker long stagger by t=0.24s"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	FPRDamageRequest PriorityHeavyRequest = ShortHeavyRequest;
	PriorityHeavyRequest.HitReaction.DurationSeconds = 0.30f;
	TestTrue(
		TEXT("Priority Heavy hit is accepted at t=0.00s"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, PriorityHeavyRequest, EnemySource));
	TickCombatFeedbackWorld(World, 0.05f);
	TestTrue(
		TEXT("Weaker long Light hit is accepted at t=0.05s"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LongLightRequest, EnemySource));
	TickCombatFeedbackWorld(World, 0.20f);
	TestTrue(TEXT("Heavy remains active at t=0.25s before its original 0.30s expiry"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TickCombatFeedbackWorld(World, 0.08f);
	TestFalse(TEXT("Weaker Light neither replaces nor extends Heavy beyond t=0.33s"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	FPRDamageRequest LongHeavyRequest = PriorityHeavyRequest;
	LongHeavyRequest.HitReaction.DurationSeconds = 1.0f;
	TestTrue(
		TEXT("Long Heavy hit is accepted before stun"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LongHeavyRequest, EnemySource));
	TestTrue(TEXT("Long Heavy is active immediately before stun application"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FPRTargetStatusEffectDefinition BriefStunDefinition(
		UPRStunStatusGameplayEffect::StaticClass(),
		0.0f,
		0.20f);
	TestTrue(
		TEXT("Stun applies over active stagger"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemySourceASC,
			StaggerTarget.AbilitySystem,
			BriefStunDefinition,
			EnemySource).IsValid());
	TestTrue(TEXT("Stun grants State.Stunned"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
	TestFalse(TEXT("Stun removes and supersedes active hit stagger"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestTrue(
		TEXT("Long Light damage remains accepted while target is stunned"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LongLightRequest, EnemySource));
	TestFalse(TEXT("A 1.0s Long Light hit during stun cannot re-add hit stagger"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TickCombatFeedbackWorld(World, 0.22f);
	TestFalse(TEXT("Suppressed 1.0s Long Light does not appear after the 0.20s stun expires"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestCombatFeedbackFloatNear(*this, TEXT("Movement restores aggregated 487 speed after superseding stun expires"), StaggerTarget.Character->GetCharacterMovement()->MaxWalkSpeed, 487.0f);

	TestTrue(
		TEXT("Long Light hit is accepted before explicit cleanup"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, StaggerTarget.AbilitySystem, LongLightRequest, EnemySource));
	TestTrue(TEXT("Stagger is active before explicit negative-effect cleanup"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(StaggerTarget.AbilitySystem);
	TestFalse(TEXT("ClearNegativeStatusEffects removes hit stagger"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestCombatFeedbackFloatNear(*this, TEXT("Explicit negative-effect cleanup restores aggregated 487 speed"), StaggerTarget.Character->GetCharacterMovement()->MaxWalkSpeed, 487.0f);

	// Legacy and periodic damage remain intentionally feedback-free and never stagger.
	ResetCombatFeedbackObservation(StaggerTarget.Feedback);
	TestTrue(
		TEXT("Legacy damage entry point remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageToTarget(
			EnemySourceASC,
			StaggerTarget.AbilitySystem,
			1.0f,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			EnemySource));
	TestFalse(TEXT("Legacy damage defaults to no stagger"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	FIntProperty* LegacyDispatchCountProperty = FindFProperty<FIntProperty>(
		StaggerTarget.Feedback->GetClass(),
		TEXT("FeedbackDispatchCount"));
	TestNotNull(TEXT("Legacy damage dispatch count remains observable"), LegacyDispatchCountProperty);
	if (LegacyDispatchCountProperty)
	{
		TestEqual(
			TEXT("Legacy damage dispatches no structured combat feedback"),
			LegacyDispatchCountProperty->GetPropertyValue_InContainer(StaggerTarget.Feedback),
			0);
	}

	UPRCombatEffectLibrary::ClearNegativeStatusEffects(StaggerTarget.AbilitySystem);
	StaggerTarget.Attributes->SetHealth(100.0f);
	StaggerTarget.Attributes->SetShield(0.0f);
	StaggerTarget.Attributes->SetPollutionResistance(0.0f);
	ResetCombatFeedbackObservation(StaggerTarget.Feedback);
	const FPRTargetStatusEffectDefinition PollutionDefinition(
		UPRPollutionStatusGameplayEffect::StaticClass(),
		2.0f,
		0.25f);
	TestTrue(
		TEXT("Pollution status applies for periodic feedback policy test"),
		UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
			EnemySourceASC,
			StaggerTarget.AbilitySystem,
			PollutionDefinition,
			EnemySource).IsValid());
	TickCombatFeedbackWorld(World, 0.05f);
	TestCombatFeedbackFloatNear(*this, TEXT("Pollution immediate tick deals 2.2 real health damage at AttackPower 10"), StaggerTarget.Attributes->GetHealth(), 97.8f);
	TestFalse(TEXT("Pollution tick defaults to no hit stagger"), StaggerTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback PollutionObservation = ObserveCombatFeedback(
		*this,
		StaggerTarget.Feedback,
		TEXT("Pollution target"));
	if (PollutionObservation.bHasResolvedDamage)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Pollution tick does not dispatch shield hit confirmation"), PollutionObservation.ResolvedDamage.ShieldDamage, 0.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Pollution tick does not dispatch health hit confirmation"), PollutionObservation.ResolvedDamage.HealthDamage, 0.0f);
	}
	if (PollutionObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Pollution tick dispatches no structured combat feedback"), PollutionObservation.FeedbackDispatchCount, 0);
	}

	// Enemy death removes an already-active stagger and the lethal hit cannot add another.
	FCombatFeedbackTestPlayer PlayerAttacker = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(20000.0f, 0.0f, 0.0f));
	APREnemyCharacter* EnemyDeathTarget = World->SpawnActor<APREnemyCharacter>(
		FVector(20100.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	UPRAbilitySystemComponent* EnemyDeathTargetASC = EnemyDeathTarget
		? EnemyDeathTarget->GetProjectRiftAbilitySystemComponent()
		: nullptr;
	TestNotNull(TEXT("Player attacker ASC initializes"), PlayerAttacker.AbilitySystem);
	TestNotNull(TEXT("Enemy death target ASC initializes"), EnemyDeathTargetASC);
	if (PlayerAttacker.AbilitySystem && EnemyDeathTargetASC && EnemyDeathTarget)
	{
		EnemyDeathTargetASC->SetNumericAttributeBase(UPRAttributeSet::GetMoveSpeedAttribute(), 333.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Enemy movement uses non-default aggregated speed 333 before stagger"), EnemyDeathTarget->GetCharacterMovement()->MaxWalkSpeed, 333.0f);
		const FPRDamageRequest EnemyLightRequest = MakeDamageRequest(
			EnemyDeathTarget,
			1.0f,
			EPRHitReactionStrength::Light,
			0.12f);
		TestTrue(
			TEXT("Enemy accepts a Light hit for movement timing"),
			UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
				PlayerAttacker.AbilitySystem,
				EnemyDeathTargetASC,
				EnemyLightRequest,
				PlayerAttacker.Character));
		TestTrue(TEXT("Enemy Light stagger is active at t=0.00s"), EnemyDeathTargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
		TestCombatFeedbackFloatNear(*this, TEXT("Enemy Light stagger stops movement at t=0.00s"), EnemyDeathTarget->GetCharacterMovement()->MaxWalkSpeed, 0.0f);
		TickCombatFeedbackWorld(World, 0.14f);
		TestFalse(TEXT("Enemy Light stagger expires by t=0.14s"), EnemyDeathTargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
		TestCombatFeedbackFloatNear(*this, TEXT("Enemy movement restores aggregated 333 speed after Light expiry"), EnemyDeathTarget->GetCharacterMovement()->MaxWalkSpeed, 333.0f);

		FPRDamageRequest EnemyHeavyRequest = MakeDamageRequest(
			EnemyDeathTarget,
			1.0f,
			EPRHitReactionStrength::Heavy,
			1.0f);
		TestTrue(TEXT("Enemy accepts a nonlethal heavy hit"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(PlayerAttacker.AbilitySystem, EnemyDeathTargetASC, EnemyHeavyRequest, PlayerAttacker.Character));
		TestTrue(TEXT("Enemy enters hit stagger before death"), EnemyDeathTargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
		FPRDamageRequest EnemyLethalRequest = EnemyHeavyRequest;
		EnemyLethalRequest.BaseDamage = 100.0f;
		TestTrue(TEXT("Enemy accepts lethal structured damage"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(PlayerAttacker.AbilitySystem, EnemyDeathTargetASC, EnemyLethalRequest, PlayerAttacker.Character));
		TestTrue(TEXT("Enemy enters authoritative dead state"), EnemyDeathTarget->IsDead());
		TestFalse(TEXT("Enemy death clears hit stagger and lethal damage does not re-add it"), EnemyDeathTargetASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	}

	// Invalid requests and friendly fire must neither mutate attributes nor dispatch observations.
	FCombatFeedbackTestPlayer InvalidTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(30000.0f, 0.0f, 0.0f));
	if (!InvalidTarget.AbilitySystem || !InvalidTarget.Attributes || !InvalidTarget.Feedback)
	{
		return false;
	}
	InvalidTarget.Attributes->SetHealth(100.0f);
	InvalidTarget.Attributes->SetShield(50.0f);
	ResetCombatFeedbackObservation(InvalidTarget.Feedback);
	FPRDamageRequest ZeroDamageRequest = MakeDamageRequest(
		InvalidTarget.Character,
		0.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestFalse(TEXT("Zero structured damage fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, ZeroDamageRequest, EnemySource));
	FPRDamageRequest NegativeDamageRequest = MakeDamageRequest(
		InvalidTarget.Character,
		-1.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestFalse(TEXT("Negative structured damage fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, NegativeDamageRequest, EnemySource));
	FPRDamageRequest InfiniteDamageRequest = MakeDamageRequest(
		InvalidTarget.Character,
		std::numeric_limits<float>::infinity(),
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestFalse(TEXT("Infinite structured damage fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, InfiniteDamageRequest, EnemySource));
	FPRDamageRequest NaNDamageRequest = MakeDamageRequest(
		InvalidTarget.Character,
		std::numeric_limits<float>::quiet_NaN(),
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestFalse(TEXT("NaN structured damage fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, NaNDamageRequest, EnemySource));
	FPRDamageRequest NaNHitPointRequest = MakeDamageRequest(
		InvalidTarget.Character,
		10.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	NaNHitPointRequest.HitResult.ImpactPoint.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("A blocking hit with a NaN impact point fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, NaNHitPointRequest, EnemySource));
	FPRDamageRequest InfiniteHitNormalRequest = MakeDamageRequest(
		InvalidTarget.Character,
		10.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	InfiniteHitNormalRequest.HitResult.ImpactNormal.Z = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("A blocking hit with an infinite impact normal fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, InfiniteHitNormalRequest, EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("Invalid damage preserves shield"), InvalidTarget.Attributes->GetShield(), 50.0f);
	TestCombatFeedbackFloatNear(*this, TEXT("Invalid damage preserves health"), InvalidTarget.Attributes->GetHealth(), 100.0f);
	TestFalse(TEXT("Invalid damage grants no stagger"), InvalidTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback InvalidObservation = ObserveCombatFeedback(*this, InvalidTarget.Feedback, TEXT("Invalid request target"));
	if (InvalidObservation.bHasResolvedDamage)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Invalid damage dispatches no shield feedback"), InvalidObservation.ResolvedDamage.ShieldDamage, 0.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Invalid damage dispatches no health feedback"), InvalidObservation.ResolvedDamage.HealthDamage, 0.0f);
	}
	if (InvalidObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Invalid damage values dispatch no combat feedback"), InvalidObservation.FeedbackDispatchCount, 0);
	}

	FPRDamageRequest ZeroReactionDurationRequest = MakeDamageRequest(
		InvalidTarget.Character,
		1.0f,
		EPRHitReactionStrength::Heavy,
		0.0f);
	TestTrue(TEXT("Valid damage with zero reaction duration remains accepted"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, ZeroReactionDurationRequest, EnemySource));
	TestFalse(TEXT("Zero reaction duration produces no stagger state"), InvalidTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	FPRDamageRequest NegativeReactionDurationRequest = ZeroReactionDurationRequest;
	NegativeReactionDurationRequest.HitReaction.DurationSeconds = -1.0f;
	TestTrue(TEXT("Valid damage with negative reaction duration remains accepted"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, NegativeReactionDurationRequest, EnemySource));
	TestFalse(TEXT("Negative reaction duration produces no stagger state"), InvalidTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	FPRDamageRequest InfiniteReactionDurationRequest = ZeroReactionDurationRequest;
	InfiniteReactionDurationRequest.HitReaction.DurationSeconds = std::numeric_limits<float>::infinity();
	TestTrue(TEXT("Valid damage with infinite reaction duration remains accepted"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, InfiniteReactionDurationRequest, EnemySource));
	TestFalse(TEXT("Infinite reaction duration produces no stagger state"), InvalidTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	FPRDamageRequest NaNReactionDurationRequest = ZeroReactionDurationRequest;
	NaNReactionDurationRequest.HitReaction.DurationSeconds = std::numeric_limits<float>::quiet_NaN();
	TestTrue(TEXT("Valid damage with NaN reaction duration remains accepted"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(EnemySourceASC, InvalidTarget.AbilitySystem, NaNReactionDurationRequest, EnemySource));
	TestFalse(TEXT("NaN reaction duration produces no stagger state"), InvalidTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestCombatFeedbackFloatNear(*this, TEXT("Four valid damage requests with invalid reaction durations still deal 4.4 shield damage"), InvalidTarget.Attributes->GetShield(), 45.6f);

	FCombatFeedbackTestPlayer FriendlyTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(30100.0f, 0.0f, 0.0f));
	if (!FriendlyTarget.AbilitySystem || !FriendlyTarget.Attributes || !FriendlyTarget.Feedback || !PlayerAttacker.AbilitySystem)
	{
		return false;
	}
	FriendlyTarget.Attributes->SetHealth(100.0f);
	FriendlyTarget.Attributes->SetShield(50.0f);
	ResetCombatFeedbackObservation(FriendlyTarget.Feedback);
	const FPRDamageRequest FriendlyRequest = MakeDamageRequest(
		FriendlyTarget.Character,
		20.0f,
		EPRHitReactionStrength::Heavy,
		1.0f);
	TestFalse(TEXT("Player friendly structured damage fails closed"), UPRCombatEffectLibrary::ApplyDamageRequestToTarget(PlayerAttacker.AbilitySystem, FriendlyTarget.AbilitySystem, FriendlyRequest, PlayerAttacker.Character));
	TestCombatFeedbackFloatNear(*this, TEXT("Friendly rejection preserves shield"), FriendlyTarget.Attributes->GetShield(), 50.0f);
	TestCombatFeedbackFloatNear(*this, TEXT("Friendly rejection preserves health"), FriendlyTarget.Attributes->GetHealth(), 100.0f);
	TestFalse(TEXT("Friendly rejection grants no stagger"), FriendlyTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback FriendlyObservation = ObserveCombatFeedback(*this, FriendlyTarget.Feedback, TEXT("Friendly target"));
	if (FriendlyObservation.bHasResolvedDamage)
	{
		TestCombatFeedbackFloatNear(*this, TEXT("Friendly rejection dispatches no shield feedback"), FriendlyObservation.ResolvedDamage.ShieldDamage, 0.0f);
		TestCombatFeedbackFloatNear(*this, TEXT("Friendly rejection dispatches no health feedback"), FriendlyObservation.ResolvedDamage.HealthDamage, 0.0f);
	}
	if (FriendlyObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Friendly rejection dispatches no combat feedback"), FriendlyObservation.FeedbackDispatchCount, 0);
	}

	// A structured damage request may opt out of all feedback without opting out of damage.
	FCombatFeedbackTestPlayer PolicyTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(30200.0f, 0.0f, 0.0f));
	if (!PolicyTarget.AbilitySystem || !PolicyTarget.Attributes || !PolicyTarget.Feedback)
	{
		return false;
	}
	PolicyTarget.Attributes->SetHealth(100.0f);
	PolicyTarget.Attributes->SetShield(50.0f);
	ResetCombatFeedbackObservation(PolicyTarget.Feedback);
	FPRDamageRequest NoFeedbackRequest;
	NoFeedbackRequest.BaseDamage = 10.0f;
	NoFeedbackRequest.DamageType = ProjectRiftGameplayTags::Damage_Type_Physical;
	TestEqual(TEXT("Default structured request policy is None"), NoFeedbackRequest.FeedbackPolicy, EPRCombatFeedbackPolicy::None);
	TestEqual(TEXT("Default structured request reaction is None"), NoFeedbackRequest.HitReaction.Strength, EPRHitReactionStrength::None);
	TestTrue(
		TEXT("Structured damage with explicit valid damage fields and default no-feedback settings is accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			PolicyTarget.AbilitySystem,
			NoFeedbackRequest,
			EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("No-feedback structured damage still settles against shield"), PolicyTarget.Attributes->GetShield(), 39.0f);
	TestFalse(TEXT("No-feedback structured damage grants no stagger"), PolicyTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback NoFeedbackObservation = ObserveCombatFeedback(*this, PolicyTarget.Feedback, TEXT("No-feedback target"));
	if (NoFeedbackObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("FeedbackPolicy None dispatches no combat feedback"), NoFeedbackObservation.FeedbackDispatchCount, 0);
	}

	// Unknown enum values must fail closed while preserving otherwise-valid damage settlement.
	PolicyTarget.Attributes->SetHealth(100.0f);
	PolicyTarget.Attributes->SetShield(50.0f);
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(PolicyTarget.AbilitySystem);
	ResetCombatFeedbackObservation(PolicyTarget.Feedback);
	FPRDamageRequest InvalidPolicyRequest = MakeDamageRequest(
		PolicyTarget.Character,
		10.0f,
		EPRHitReactionStrength::Light,
		0.50f);
	InvalidPolicyRequest.FeedbackPolicy = static_cast<EPRCombatFeedbackPolicy>(255);
	TestTrue(
		TEXT("Valid damage with unknown feedback policy remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			PolicyTarget.AbilitySystem,
			InvalidPolicyRequest,
			EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("Unknown feedback policy still permits damage settlement"), PolicyTarget.Attributes->GetShield(), 39.0f);
	TestFalse(TEXT("Unknown feedback policy cannot grant stagger"), PolicyTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback InvalidPolicyObservation = ObserveCombatFeedback(*this, PolicyTarget.Feedback, TEXT("Unknown-policy target"));
	if (InvalidPolicyObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Unknown feedback policy dispatches no combat feedback"), InvalidPolicyObservation.FeedbackDispatchCount, 0);
	}

	PolicyTarget.Attributes->SetHealth(100.0f);
	PolicyTarget.Attributes->SetShield(50.0f);
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(PolicyTarget.AbilitySystem);
	ResetCombatFeedbackObservation(PolicyTarget.Feedback);
	FPRDamageRequest InvalidStrengthRequest = MakeDamageRequest(
		PolicyTarget.Character,
		10.0f,
		static_cast<EPRHitReactionStrength>(255),
		0.50f);
	TestTrue(
		TEXT("Valid damage with unknown reaction strength remains accepted"),
		UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
			EnemySourceASC,
			PolicyTarget.AbilitySystem,
			InvalidStrengthRequest,
			EnemySource));
	TestCombatFeedbackFloatNear(*this, TEXT("Unknown reaction strength still permits damage settlement"), PolicyTarget.Attributes->GetShield(), 39.0f);
	TestFalse(TEXT("Unknown reaction strength cannot grant stagger"), PolicyTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback InvalidStrengthObservation = ObserveCombatFeedback(*this, PolicyTarget.Feedback, TEXT("Unknown-strength target"));
	if (InvalidStrengthObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Unknown reaction strength dispatches no combat feedback"), InvalidStrengthObservation.FeedbackDispatchCount, 0);
	}

	// Both the damage entry point and direct dispatch are hard authority boundaries.
	FCombatFeedbackTestPlayer AuthorityTarget = SpawnCombatFeedbackTestPlayer(
		World,
		FVector(30300.0f, 0.0f, 0.0f));
	if (!AuthorityTarget.AbilitySystem || !AuthorityTarget.Attributes || !AuthorityTarget.Feedback)
	{
		return false;
	}
	AuthorityTarget.Attributes->SetHealth(100.0f);
	AuthorityTarget.Attributes->SetShield(50.0f);
	ResetCombatFeedbackObservation(AuthorityTarget.Feedback);
	const FPRDamageRequest AuthorityRequest = MakeDamageRequest(
		AuthorityTarget.Character,
		10.0f,
		EPRHitReactionStrength::Light,
		0.50f);
	EnemySource->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Authority guard source is a real SimulatedProxy actor"), EnemySource->HasAuthority());
	const bool bNonAuthorityDamageAccepted = UPRCombatEffectLibrary::ApplyDamageRequestToTarget(
		EnemySourceASC,
		AuthorityTarget.AbilitySystem,
		AuthorityRequest,
		EnemySource);
	EnemySource->SetRole(ROLE_Authority);
	TestFalse(TEXT("Structured damage rejects a SimulatedProxy source"), bNonAuthorityDamageAccepted);
	TestCombatFeedbackFloatNear(*this, TEXT("Non-authority apply preserves target shield"), AuthorityTarget.Attributes->GetShield(), 50.0f);
	TestCombatFeedbackFloatNear(*this, TEXT("Non-authority apply preserves target health"), AuthorityTarget.Attributes->GetHealth(), 100.0f);
	TestFalse(TEXT("Non-authority apply grants no stagger"), AuthorityTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback NonAuthorityApplyObservation = ObserveCombatFeedback(*this, AuthorityTarget.Feedback, TEXT("Non-authority apply target"));
	if (NonAuthorityApplyObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Non-authority apply dispatches no combat feedback"), NonAuthorityApplyObservation.FeedbackDispatchCount, 0);
	}

	UPRCombatEffectLibrary::ClearNegativeStatusEffects(AuthorityTarget.AbilitySystem);
	ResetCombatFeedbackObservation(AuthorityTarget.Feedback);
	FPRResolvedDamage DirectResolvedDamage;
	DirectResolvedDamage.ShieldDamage = 10.0f;
	FGameplayEffectContextHandle DirectEffectContext = EnemySourceASC->MakeEffectContext();
	DirectEffectContext.AddInstigator(EnemySource, EnemySource);
	EnemySource->SetRole(ROLE_SimulatedProxy);
	UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
		EnemySourceASC,
		AuthorityTarget.AbilitySystem,
		AuthorityRequest,
		DirectResolvedDamage,
		DirectEffectContext);
	EnemySource->SetRole(ROLE_Authority);
	TestFalse(TEXT("Direct non-authority dispatch grants no stagger"), AuthorityTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	const FObservedCombatFeedback NonAuthorityDispatchObservation = ObserveCombatFeedback(*this, AuthorityTarget.Feedback, TEXT("Non-authority direct-dispatch target"));
	if (NonAuthorityDispatchObservation.bHasDispatchCount)
	{
		TestEqual(TEXT("Direct non-authority dispatch records nothing"), NonAuthorityDispatchObservation.FeedbackDispatchCount, 0);
	}

	// The public post-settlement dispatcher must independently fail closed.
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(AuthorityTarget.AbilitySystem);
	ResetCombatFeedbackObservation(AuthorityTarget.Feedback);
	FPRResolvedDamage ZeroResolvedDamage;
	UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
		EnemySourceASC,
		AuthorityTarget.AbilitySystem,
		AuthorityRequest,
		ZeroResolvedDamage,
		DirectEffectContext);
	TestEqual(TEXT("Direct zero-damage dispatch records no target feedback"), AuthorityTarget.Feedback->FeedbackDispatchCount, 0);
	TestFalse(TEXT("Direct zero-damage dispatch grants no stagger"), AuthorityTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	FPRResolvedDamage NaNResolvedDamage;
	NaNResolvedDamage.HealthDamage = std::numeric_limits<float>::quiet_NaN();
	UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
		EnemySourceASC,
		AuthorityTarget.AbilitySystem,
		AuthorityRequest,
		NaNResolvedDamage,
		DirectEffectContext);
	TestEqual(TEXT("Direct NaN resolved dispatch records no target feedback"), AuthorityTarget.Feedback->FeedbackDispatchCount, 0);
	TestFalse(TEXT("Direct NaN resolved dispatch grants no stagger"), AuthorityTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	ResetCombatFeedbackObservation(FriendlyTarget.Feedback);
	FPRResolvedDamage FriendlyResolvedDamage;
	FriendlyResolvedDamage.ShieldDamage = 5.0f;
	FGameplayEffectContextHandle FriendlyEffectContext = PlayerAttacker.AbilitySystem->MakeEffectContext();
	FriendlyEffectContext.AddInstigator(PlayerAttacker.Character, PlayerAttacker.Character);
	UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
		PlayerAttacker.AbilitySystem,
		FriendlyTarget.AbilitySystem,
		FriendlyRequest,
		FriendlyResolvedDamage,
		FriendlyEffectContext);
	TestEqual(TEXT("Direct friendly dispatch records no target feedback"), FriendlyTarget.Feedback->FeedbackDispatchCount, 0);
	TestFalse(TEXT("Direct friendly dispatch grants no stagger"), FriendlyTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	const int32 DownedDispatchCountBefore = ResolutionTarget.Feedback->FeedbackDispatchCount;
	FPRResolvedDamage InactiveResolvedDamage;
	InactiveResolvedDamage.HealthDamage = 5.0f;
	InactiveResolvedDamage.bLethal = true;
	UPRCombatEffectLibrary::DispatchResolvedDamageFeedback(
		EnemySourceASC,
		ResolutionTarget.AbilitySystem,
		LethalResolutionRequest,
		InactiveResolvedDamage,
		DirectEffectContext);
	TestEqual(
		TEXT("Direct dispatch to an already downed target records no new feedback"),
		ResolutionTarget.Feedback->FeedbackDispatchCount,
		DownedDispatchCountBefore);
	TestFalse(TEXT("Direct dispatch to an already downed target cannot re-add stagger"), ResolutionTarget.AbilitySystem->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));

	return true;
}

#endif
