#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "GameplayCues/PRGameplayCueNotifyBase.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Core/PRGameplayTags.h"
#include "GameplayEffect.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCombatGameplayCueTest,
	"ProjectRift.CombatFeedback.GameplayCues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool HasExactGameplayCue(const UGameplayEffect* Effect, const FGameplayTag ExpectedTag)
{
	if (!Effect || !ExpectedTag.IsValid())
	{
		return false;
	}

	for (const FGameplayEffectCue& Cue : Effect->GameplayCues)
	{
		if (Cue.GameplayCueTags.HasTagExact(ExpectedTag))
		{
			return true;
		}
	}
	return false;
}

FGameplayTagContainer ReadTagContainerProperty(
	FAutomationTestBase& Test,
	UPRCombatFeedbackComponent* Feedback,
	const FName PropertyName)
{
	FGameplayTagContainer Result;
	FStructProperty* Property = Feedback
		? FindFProperty<FStructProperty>(Feedback->GetClass(), PropertyName)
		: nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("Combat feedback exposes %s"), *PropertyName.ToString()),
		Property);
	if (!Property)
	{
		return Result;
	}

	Test.TestEqual(
		FString::Printf(TEXT("%s uses FGameplayTagContainer"), *PropertyName.ToString()),
		Property->Struct.Get(),
		FGameplayTagContainer::StaticStruct());
	Test.TestTrue(
		FString::Printf(TEXT("%s is BlueprintReadOnly"), *PropertyName.ToString()),
		Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	if (Property->Struct == FGameplayTagContainer::StaticStruct())
	{
		Result = *Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Feedback);
	}
	return Result;
}

void RequireBlueprintReadOnlyProperty(
	FAutomationTestBase& Test,
	const UClass* OwnerClass,
	const FName PropertyName,
	const FFieldClass* ExpectedClass)
{
	FProperty* Property = OwnerClass ? OwnerClass->FindPropertyByName(PropertyName) : nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("Combat feedback exposes %s"), *PropertyName.ToString()),
		Property);
	if (Property)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s has the expected reflected type"), *PropertyName.ToString()),
			Property->GetClass() == ExpectedClass);
		Test.TestTrue(
			FString::Printf(TEXT("%s is BlueprintReadOnly"), *PropertyName.ToString()),
			Property->HasAnyPropertyFlags(CPF_BlueprintVisible) && Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	}
}

int32 ReadIntProperty(
	FAutomationTestBase& Test,
	UPRCombatFeedbackComponent* Feedback,
	const FName PropertyName)
{
	FIntProperty* Property = Feedback
		? FindFProperty<FIntProperty>(Feedback->GetClass(), PropertyName)
		: nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("Combat feedback exposes readable %s"), *PropertyName.ToString()),
		Property);
	return Property ? Property->GetPropertyValue_InContainer(Feedback) : 0;
}

bool ReadBoolProperty(
	FAutomationTestBase& Test,
	UPRCombatFeedbackComponent* Feedback,
	const FName PropertyName)
{
	FBoolProperty* Property = Feedback
		? FindFProperty<FBoolProperty>(Feedback->GetClass(), PropertyName)
		: nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("Combat feedback exposes readable %s"), *PropertyName.ToString()),
		Property);
	return Property && Property->GetPropertyValue_InContainer(Feedback);
}

FGameplayTag ReadGameplayTagProperty(
	FAutomationTestBase& Test,
	UPRCombatFeedbackComponent* Feedback,
	const FName PropertyName)
{
	FStructProperty* Property = Feedback
		? FindFProperty<FStructProperty>(Feedback->GetClass(), PropertyName)
		: nullptr;
	Test.TestNotNull(
		FString::Printf(TEXT("Combat feedback exposes readable %s"), *PropertyName.ToString()),
		Property);
	if (!Property || Property->Struct != FGameplayTag::StaticStruct())
	{
		return FGameplayTag();
	}
	return *Property->ContainerPtrToValuePtr<FGameplayTag>(Feedback);
}
}

bool FPRCombatGameplayCueTest::RunTest(const FString& Parameters)
{
	struct FCueAssetContract
	{
		const TCHAR* ClassPath;
		FGameplayTag CueTag;
	};

	const TArray<FCueAssetContract> CueAssetContracts =
	{
		{ TEXT("/Game/ProjectRift/GameplayCues/Impact/GC_Combat_Impact_Shield.GC_Combat_Impact_Shield_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield },
		{ TEXT("/Game/ProjectRift/GameplayCues/Impact/GC_Combat_Impact_ShieldBreak.GC_Combat_Impact_ShieldBreak_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak },
		{ TEXT("/Game/ProjectRift/GameplayCues/Impact/GC_Combat_Impact_Health.GC_Combat_Impact_Health_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health },
		{ TEXT("/Game/ProjectRift/GameplayCues/Impact/GC_Combat_Impact_Lethal.GC_Combat_Impact_Lethal_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal },
		{ TEXT("/Game/ProjectRift/GameplayCues/Status/GC_Combat_Status_Polluted.GC_Combat_Status_Polluted_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted },
		{ TEXT("/Game/ProjectRift/GameplayCues/Status/GC_Combat_Status_Slowed.GC_Combat_Status_Slowed_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed },
		{ TEXT("/Game/ProjectRift/GameplayCues/Status/GC_Combat_Status_Stunned.GC_Combat_Status_Stunned_C"), ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned },
	};

	for (const FCueAssetContract& Contract : CueAssetContracts)
	{
		UClass* CueClass = LoadObject<UClass>(nullptr, Contract.ClassPath);
		TestNotNull(FString::Printf(TEXT("GameplayCue class loads: %s"), Contract.ClassPath), CueClass);
		if (CueClass)
		{
			TestTrue(
				FString::Printf(TEXT("GameplayCue derives from ProjectRift base: %s"), Contract.ClassPath),
				CueClass->IsChildOf(UPRGameplayCueNotifyBase::StaticClass()));
			const UPRGameplayCueNotifyBase* CueCDO = Cast<UPRGameplayCueNotifyBase>(CueClass->GetDefaultObject());
			TestEqual(
				FString::Printf(TEXT("GameplayCue keeps the exact tag: %s"), Contract.ClassPath),
				CueCDO ? CueCDO->GameplayCueTag : FGameplayTag(),
				Contract.CueTag);
		}
	}

	TestNotNull(
		TEXT("Combat feedback overlay material loads from its stable ProjectRift path"),
		LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/ProjectRift/GameplayCues/Materials/M_CombatFeedbackOverlay.M_CombatFeedbackOverlay")));
	TestNotNull(
		TEXT("Combat impact placeholder particle loads from its stable ProjectRift path"),
		LoadObject<UParticleSystem>(
			nullptr,
			TEXT("/Game/ProjectRift/GameplayCues/FX/P_CombatImpactPlaceholder.P_CombatImpactPlaceholder")));

	const UClass* FeedbackClass = UPRCombatFeedbackComponent::StaticClass();
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("LastDamageCueTags"), FStructProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("LastHandledCueTag"), FStructProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("GameplayCueDispatchCount"), FIntProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("GameplayCueHandledCount"), FIntProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("LocalHitPulseCount"), FIntProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("LastDamageFeedbackSequence"), FIntProperty::StaticClass());
	RequireBlueprintReadOnlyProperty(*this, FeedbackClass, TEXT("ActiveStatusCueTags"), FStructProperty::StaticClass());
	const FStructProperty* LastHandledCueTagProperty = FindFProperty<FStructProperty>(FeedbackClass, TEXT("LastHandledCueTag"));
	if (LastHandledCueTagProperty)
	{
		TestEqual(
			TEXT("LastHandledCueTag uses FGameplayTag"),
			LastHandledCueTagProperty->Struct.Get(),
			FGameplayTag::StaticStruct());
	}
	const FStructProperty* ActiveStatusCueTagsProperty = FindFProperty<FStructProperty>(FeedbackClass, TEXT("ActiveStatusCueTags"));
	if (ActiveStatusCueTagsProperty)
	{
		TestEqual(
			TEXT("ActiveStatusCueTags uses FGameplayTagContainer"),
			ActiveStatusCueTagsProperty->Struct.Get(),
			FGameplayTagContainer::StaticStruct());
	}

	const UPRPollutionStatusGameplayEffect* PollutionEffect = GetDefault<UPRPollutionStatusGameplayEffect>();
	const UPRSlowStatusGameplayEffect* SlowEffect = GetDefault<UPRSlowStatusGameplayEffect>();
	const UPRStunStatusGameplayEffect* StunEffect = GetDefault<UPRStunStatusGameplayEffect>();
	TestTrue(
		TEXT("Pollution status declares its persistent GameplayCue"),
		HasExactGameplayCue(PollutionEffect, ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted));
	TestTrue(
		TEXT("Slow status declares its persistent GameplayCue"),
		HasExactGameplayCue(SlowEffect, ProjectRiftGameplayTags::GameplayCue_Combat_Status_Slowed));
	TestTrue(
		TEXT("Stun status declares its persistent GameplayCue"),
		HasExactGameplayCue(StunEffect, ProjectRiftGameplayTags::GameplayCue_Combat_Status_Stunned));

	UPRCombatFeedbackComponent* Feedback = NewObject<UPRCombatFeedbackComponent>(GetTransientPackage());
	TestNotNull(TEXT("Transient feedback component can be created without visual assets"), Feedback);
	if (!Feedback)
	{
		return false;
	}

	FPRResolvedDamage ShieldOnly;
	ShieldOnly.ShieldDamage = 8.0f;
	Feedback->RecordResolvedDamage(ShieldOnly, FGameplayEffectContextHandle());
	FGameplayTagContainer DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("Shield-only damage selects the Shield cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield));
	TestEqual(TEXT("Shield-only damage selects exactly one impact cue"), DamageCueTags.Num(), 1);

	FPRResolvedDamage ShieldBreakOnly;
	ShieldBreakOnly.ShieldDamage = 8.0f;
	ShieldBreakOnly.bShieldBroken = true;
	Feedback->RecordResolvedDamage(ShieldBreakOnly, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("A shield break without overflow selects the ShieldBreak cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak));
	TestFalse(TEXT("A shield break suppresses the ordinary Shield cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield));
	TestEqual(TEXT("A shield break without overflow selects exactly one impact cue"), DamageCueTags.Num(), 1);

	FPRResolvedDamage HealthOnly;
	HealthOnly.HealthDamage = 6.0f;
	Feedback->RecordResolvedDamage(HealthOnly, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("Non-lethal health damage selects the Health cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health));
	TestEqual(TEXT("Non-lethal health damage selects exactly one impact cue"), DamageCueTags.Num(), 1);

	FPRResolvedDamage ShieldBreakOverflow;
	ShieldBreakOverflow.ShieldDamage = 5.0f;
	ShieldBreakOverflow.HealthDamage = 7.0f;
	ShieldBreakOverflow.bShieldBroken = true;
	Feedback->RecordResolvedDamage(ShieldBreakOverflow, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("Shield break selects the ShieldBreak cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak));
	TestTrue(TEXT("Shield overflow selects a separate Health cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health));
	TestEqual(TEXT("Shield overflow selects exactly two impact cues"), DamageCueTags.Num(), 2);

	FPRResolvedDamage LethalHealth;
	LethalHealth.HealthDamage = 12.0f;
	LethalHealth.bLethal = true;
	Feedback->RecordResolvedDamage(LethalHealth, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("Lethal health damage selects the Lethal cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal));
	TestEqual(TEXT("Lethal health damage selects exactly one terminal cue"), DamageCueTags.Num(), 1);

	FPRResolvedDamage ShieldBreakLethalOverflow;
	ShieldBreakLethalOverflow.ShieldDamage = 4.0f;
	ShieldBreakLethalOverflow.HealthDamage = 9.0f;
	ShieldBreakLethalOverflow.bShieldBroken = true;
	ShieldBreakLethalOverflow.bLethal = true;
	Feedback->RecordResolvedDamage(ShieldBreakLethalOverflow, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("Lethal shield overflow preserves the ShieldBreak cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak));
	TestTrue(TEXT("Lethal shield overflow selects Lethal instead of Health"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Lethal));
	TestFalse(TEXT("Lethal shield overflow suppresses the non-terminal Health cue"), DamageCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health));
	TestEqual(TEXT("Lethal shield overflow selects exactly two impact cues"), DamageCueTags.Num(), 2);

	FPRResolvedDamage EmptyResult;
	Feedback->RecordResolvedDamage(EmptyResult, FGameplayEffectContextHandle());
	DamageCueTags = ReadTagContainerProperty(*this, Feedback, TEXT("LastDamageCueTags"));
	TestTrue(TEXT("An empty resolved result selects no impact cue"), DamageCueTags.IsEmpty());

	UPRCombatFeedbackComponent* SequenceFeedback = NewObject<UPRCombatFeedbackComponent>(GetTransientPackage());
	TestNotNull(TEXT("Feedback sequence contract has an isolated component"), SequenceFeedback);
	if (!SequenceFeedback)
	{
		return false;
	}
	FPRResolvedDamage SequencedOverflow;
	SequencedOverflow.ShieldDamage = 2.0f;
	SequencedOverflow.HealthDamage = 3.0f;
	SequencedOverflow.bShieldBroken = true;
	SequenceFeedback->RecordResolvedDamage(SequencedOverflow, FGameplayEffectContextHandle());
	const int32 FirstDamageFeedbackSequence = ReadIntProperty(*this, SequenceFeedback, TEXT("LastDamageFeedbackSequence"));
	TestTrue(TEXT("A resolved damage dispatch allocates a positive Cue sequence"), FirstDamageFeedbackSequence > 0);
	const FGameplayTagContainer FirstSequencedCueTags = ReadTagContainerProperty(*this, SequenceFeedback, TEXT("LastDamageCueTags"));
	TestEqual(TEXT("The first sequence owns both overflow Cue tags"), FirstSequencedCueTags.Num(), 2);
	FPRResolvedDamage SequencedNextHit;
	SequencedNextHit.HealthDamage = 1.0f;
	SequenceFeedback->RecordResolvedDamage(SequencedNextHit, FGameplayEffectContextHandle());
	TestEqual(
		TEXT("The next resolved damage dispatch advances the Cue sequence exactly once"),
		ReadIntProperty(*this, SequenceFeedback, TEXT("LastDamageFeedbackSequence")),
		FirstDamageFeedbackSequence + 1);
	for (int32 SequenceIndex = 2; SequenceIndex < 31; ++SequenceIndex)
	{
		SequenceFeedback->RecordResolvedDamage(SequencedNextHit, FGameplayEffectContextHandle());
	}
	TestEqual(
		TEXT("The network-safe Cue sequence reaches the 5-bit ceiling without exceeding it"),
		ReadIntProperty(*this, SequenceFeedback, TEXT("LastDamageFeedbackSequence")),
		31);
	SequenceFeedback->RecordResolvedDamage(SequencedNextHit, FGameplayEffectContextHandle());
	TestEqual(
		TEXT("The 32nd rapid resolved hit wraps the network-safe Cue sequence to one"),
		ReadIntProperty(*this, SequenceFeedback, TEXT("LastDamageFeedbackSequence")),
		1);

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("GameplayCue lifecycle test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}
	AActor* CueTarget = World->SpawnActor<AActor>();
	UPRCombatFeedbackComponent* CueFeedback = CueTarget
		? NewObject<UPRCombatFeedbackComponent>(CueTarget)
		: nullptr;
	TestNotNull(TEXT("Temporary GameplayCue target actor is created"), CueTarget);
	TestNotNull(TEXT("Temporary target owns a feedback component without Mesh or visual assets"), CueFeedback);
	if (!CueTarget || !CueFeedback)
	{
		return false;
	}
	CueTarget->AddInstanceComponent(CueFeedback);
	CueFeedback->RegisterComponent();

	AActor* DirectionTarget = World->SpawnActor<AActor>();
	UPRCombatFeedbackComponent* DirectionFeedback = DirectionTarget
		? NewObject<UPRCombatFeedbackComponent>(DirectionTarget)
		: nullptr;
	TestNotNull(TEXT("Synthetic direction test target is created"), DirectionTarget);
	TestNotNull(TEXT("Synthetic direction test target owns isolated feedback"), DirectionFeedback);
	if (DirectionTarget && DirectionFeedback)
	{
		DirectionTarget->AddInstanceComponent(DirectionFeedback);
		DirectionFeedback->RegisterComponent();
	}
	AActor* BehindInstigator = World->SpawnActor<AActor>(FVector(-100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Synthetic direction test source is created behind the Cue target"), BehindInstigator);
	if (DirectionTarget && DirectionFeedback && BehindInstigator)
	{
		DirectionTarget->SetActorLocation(FVector::ZeroVector);
		DirectionTarget->SetActorRotation(FRotator::ZeroRotator);
		FGameplayEffectContext* BehindContextData = new FGameplayEffectContext();
		BehindContextData->AddInstigator(BehindInstigator, BehindInstigator);
		FGameplayCueParameters BehindParameters{ FGameplayEffectContextHandle(BehindContextData) };
		BehindParameters.Location = DirectionTarget->GetActorLocation();
		BehindParameters.Normal = FVector::UpVector;
		BehindParameters.RawMagnitude = 1.0f;
		BehindParameters.NormalizedMagnitude = 1.0f;
		DirectionFeedback->HandleGameplayCue(
			ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health,
			EGameplayCueEvent::Executed,
			BehindParameters);
		TestTrue(
			TEXT("A synthetic hit at the target origin still classifies a source behind the target as a back impact"),
			ReadBoolProperty(*this, DirectionFeedback, TEXT("bLastImpactFromBack")));
	}

	UPRGameplayCueNotifyBase* CueNotify = GetMutableDefault<UPRGameplayCueNotifyBase>();
	TestNotNull(TEXT("Abstract ProjectRift GameplayCue notify CDO is available for native lifecycle routing"), CueNotify);
	if (!CueNotify)
	{
		return false;
	}

	const FGameplayTag SavedCueTag = CueNotify->GameplayCueTag;
	const FGameplayTag PollutedCueTag = ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted;
	const FGameplayTag ShieldCueTag = ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Shield;
	FGameplayCueParameters CueParameters;
	CueNotify->GameplayCueTag = PollutedCueTag;
	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::OnActive, CueParameters);
	TestEqual(TEXT("Polluted OnActive is handled exactly once"), ReadIntProperty(*this, CueFeedback, TEXT("GameplayCueHandledCount")), 1);
	TestEqual(TEXT("Polluted OnActive records the handled tag"), ReadGameplayTagProperty(*this, CueFeedback, TEXT("LastHandledCueTag")), PollutedCueTag);
	FGameplayTagContainer ActiveCueTags = ReadTagContainerProperty(*this, CueFeedback, TEXT("ActiveStatusCueTags"));
	TestTrue(TEXT("Polluted OnActive adds the persistent status Cue"), ActiveCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted));
	TestEqual(TEXT("Polluted OnActive stores one active status Cue instance"), ActiveCueTags.Num(), 1);

	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::WhileActive, CueParameters);
	TestEqual(TEXT("Polluted WhileActive is handled once after OnActive"), ReadIntProperty(*this, CueFeedback, TEXT("GameplayCueHandledCount")), 2);
	ActiveCueTags = ReadTagContainerProperty(*this, CueFeedback, TEXT("ActiveStatusCueTags"));
	TestEqual(TEXT("Polluted WhileActive preserves a single status Cue instance"), ActiveCueTags.Num(), 1);

	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::Removed, CueParameters);
	TestEqual(TEXT("Polluted OnRemove is handled once after activation events"), ReadIntProperty(*this, CueFeedback, TEXT("GameplayCueHandledCount")), 3);
	ActiveCueTags = ReadTagContainerProperty(*this, CueFeedback, TEXT("ActiveStatusCueTags"));
	TestFalse(TEXT("Polluted OnRemove removes the persistent status Cue"), ActiveCueTags.HasTagExact(ProjectRiftGameplayTags::GameplayCue_Combat_Status_Polluted));
	TestTrue(TEXT("Polluted OnRemove leaves no active status Cue"), ActiveCueTags.IsEmpty());

	CueNotify->GameplayCueTag = ShieldCueTag;
	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::Executed, CueParameters);
	TestEqual(TEXT("Shield Executed is handled after the status lifecycle"), ReadIntProperty(*this, CueFeedback, TEXT("GameplayCueHandledCount")), 4);
	TestEqual(TEXT("Shield Executed records the impact Cue tag"), ReadGameplayTagProperty(*this, CueFeedback, TEXT("LastHandledCueTag")), ShieldCueTag);
	ActiveCueTags = ReadTagContainerProperty(*this, CueFeedback, TEXT("ActiveStatusCueTags"));
	TestTrue(TEXT("Instant Shield impact never becomes an active status Cue"), ActiveCueTags.IsEmpty());

	FGameplayCueParameters MatchedParameters;
	MatchedParameters.MatchedTagName = PollutedCueTag;
	CueNotify->GameplayCueTag = ShieldCueTag;
	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::OnActive, MatchedParameters);
	TestEqual(TEXT("MatchedTagName OnActive is handled after the fallback lifecycle"), ReadIntProperty(*this, CueFeedback, TEXT("GameplayCueHandledCount")), 5);
	TestEqual(TEXT("MatchedTagName takes priority over the notify CDO fallback tag"), ReadGameplayTagProperty(*this, CueFeedback, TEXT("LastHandledCueTag")), PollutedCueTag);
	ActiveCueTags = ReadTagContainerProperty(*this, CueFeedback, TEXT("ActiveStatusCueTags"));
	TestTrue(TEXT("MatchedTagName activates Polluted rather than the Shield CDO tag"), ActiveCueTags.HasTagExact(PollutedCueTag));
	TestFalse(TEXT("An impact CDO fallback never leaks into persistent status state"), ActiveCueTags.HasTagExact(ShieldCueTag));
	CueNotify->HandleGameplayCue(CueTarget, EGameplayCueEvent::Removed, MatchedParameters);

	AActor* PulseTarget = World->SpawnActor<AActor>();
	UPRCombatFeedbackComponent* PulseFeedback = PulseTarget
		? NewObject<UPRCombatFeedbackComponent>(PulseTarget)
		: nullptr;
	TestNotNull(TEXT("Temporary local-pulse target actor is created"), PulseTarget);
	TestNotNull(TEXT("Temporary local-pulse target owns a feedback component"), PulseFeedback);
	if (!PulseTarget || !PulseFeedback)
	{
		CueNotify->GameplayCueTag = SavedCueTag;
		return false;
	}
	PulseTarget->AddInstanceComponent(PulseFeedback);
	PulseFeedback->RegisterComponent();

	FPRResolvedDamage PulseOverflow;
	PulseOverflow.ShieldDamage = 4.0f;
	PulseOverflow.HealthDamage = 6.0f;
	PulseOverflow.bShieldBroken = true;
	PulseFeedback->RecordResolvedDamage(PulseOverflow, FGameplayEffectContextHandle());
	FGameplayCueParameters FallbackParameters;
	const FGameplayTag ShieldBreakCueTag = ProjectRiftGameplayTags::GameplayCue_Combat_Impact_ShieldBreak;
	const FGameplayTag HealthCueTag = ProjectRiftGameplayTags::GameplayCue_Combat_Impact_Health;
	CueNotify->GameplayCueTag = ShieldBreakCueTag;
	CueNotify->HandleGameplayCue(PulseTarget, EGameplayCueEvent::Executed, FallbackParameters);
	TestEqual(TEXT("First impact Cue creates one local hit pulse"), ReadIntProperty(*this, PulseFeedback, TEXT("LocalHitPulseCount")), 1);
	CueNotify->GameplayCueTag = HealthCueTag;
	CueNotify->HandleGameplayCue(PulseTarget, EGameplayCueEvent::Executed, FallbackParameters);
	TestEqual(TEXT("ShieldBreak and Health Cues from one resolved hit share one local pulse"), ReadIntProperty(*this, PulseFeedback, TEXT("LocalHitPulseCount")), 1);

	FPRResolvedDamage NextPulseHit;
	NextPulseHit.HealthDamage = 3.0f;
	PulseFeedback->RecordResolvedDamage(NextPulseHit, FGameplayEffectContextHandle());
	CueNotify->HandleGameplayCue(PulseTarget, EGameplayCueEvent::Executed, FallbackParameters);
	TestEqual(TEXT("A later resolved hit can create a new local pulse"), ReadIntProperty(*this, PulseFeedback, TEXT("LocalHitPulseCount")), 2);

	AActor* RemotePulseTarget = World->SpawnActor<AActor>();
	UPRCombatFeedbackComponent* RemotePulseFeedback = RemotePulseTarget
		? NewObject<UPRCombatFeedbackComponent>(RemotePulseTarget)
		: nullptr;
	TestNotNull(TEXT("Remote Cue-only pulse target actor is created"), RemotePulseTarget);
	TestNotNull(TEXT("Remote Cue-only target owns a feedback component"), RemotePulseFeedback);
	if (!RemotePulseTarget || !RemotePulseFeedback)
	{
		CueNotify->GameplayCueTag = SavedCueTag;
		return false;
	}
	RemotePulseTarget->AddInstanceComponent(RemotePulseFeedback);
	RemotePulseFeedback->RegisterComponent();

	FGameplayCueParameters RemoteSequence30;
	RemoteSequence30.AbilityLevel = 30;
	RemoteSequence30.RawMagnitude = 2.0f;
	RemotePulseFeedback->HandleGameplayCue(ShieldBreakCueTag, EGameplayCueEvent::Executed, RemoteSequence30);
	++GFrameCounter;
	RemotePulseFeedback->HandleGameplayCue(HealthCueTag, EGameplayCueEvent::Executed, RemoteSequence30);
	TestEqual(
		TEXT("Remote ShieldBreak and Health Cues with one stable sequence share one pulse across frames"),
		ReadIntProperty(*this, RemotePulseFeedback, TEXT("LocalHitPulseCount")),
		1);

	FGameplayCueParameters RemoteSequence31;
	RemoteSequence31.AbilityLevel = 31;
	RemoteSequence31.RawMagnitude = 1.0f;
	RemotePulseFeedback->HandleGameplayCue(HealthCueTag, EGameplayCueEvent::Executed, RemoteSequence31);
	TestEqual(
		TEXT("A distinct remote sequence creates a second pulse in the current frame"),
		ReadIntProperty(*this, RemotePulseFeedback, TEXT("LocalHitPulseCount")),
		2);
	FGameplayCueParameters RemoteSequence1;
	RemoteSequence1.AbilityLevel = 1;
	RemoteSequence1.RawMagnitude = 1.0f;
	RemotePulseFeedback->HandleGameplayCue(HealthCueTag, EGameplayCueEvent::Executed, RemoteSequence1);
	TestEqual(
		TEXT("Another distinct remote sequence creates a third pulse in the same frame"),
		ReadIntProperty(*this, RemotePulseFeedback, TEXT("LocalHitPulseCount")),
		3);
	CueNotify->GameplayCueTag = SavedCueTag;

	return true;
}

#endif
