#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRCombatFeedbackContractTest,
	"ProjectRift.GAS.CombatFeedback.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
template <typename PropertyType>
PropertyType* RequireProperty(FAutomationTestBase& Test, UStruct* Owner, const TCHAR* Name)
{
	PropertyType* Property = Owner ? FindFProperty<PropertyType>(Owner, Name) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Owner), Name), Property);
	return Property;
}

UScriptStruct* RequireStruct(FAutomationTestBase& Test, const TCHAR* Name)
{
	UScriptStruct* Struct = FindObject<UScriptStruct>(
		nullptr,
		*FString::Printf(TEXT("/Script/ProjectA.%s"), Name));
	Test.TestNotNull(FString::Printf(TEXT("%s is registered"), Name), Struct);
	return Struct;
}

UClass* RequireClass(FAutomationTestBase& Test, const TCHAR* Label, const TCHAR* ClassPath)
{
	UClass* Class = FindObject<UClass>(nullptr, ClassPath);
	Test.TestNotNull(FString::Printf(TEXT("%s exists"), Label), Class);
	return Class;
}

void RequireFunction(FAutomationTestBase& Test, UClass* Owner, const TCHAR* Name)
{
	if (Owner)
	{
		Test.TestNotNull(
			FString::Printf(TEXT("%s exposes %s"), *Owner->GetName(), Name),
			Owner->FindFunctionByName(Name));
	}
}

FGameplayTag RequireGameplayTag(FAutomationTestBase& Test, const TCHAR* Name)
{
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(Name), false);
	Test.TestTrue(FString::Printf(TEXT("%s is registered"), Name), Tag.IsValid());
	if (Tag.IsValid())
	{
		Test.TestEqual(FString::Printf(TEXT("%s keeps its contract name"), Name), Tag.ToString(), FString(Name));
	}
	return Tag;
}

FString NormalizeCueNotifyPath(FString Path)
{
	Path.TrimStartAndEndInline();
	Path.TrimQuotesInline();
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
	{
		Path.LeftChopInline(1);
	}
	return Path;
}

void TestActorHasFeedbackComponent(
	FAutomationTestBase& Test,
	const TCHAR* ActorLabel,
	const TCHAR* ActorClassPath,
	UClass* FeedbackComponentClass)
{
	if (!FeedbackComponentClass)
	{
		return;
	}

	UClass* ActorClass = RequireClass(Test, ActorLabel, ActorClassPath);
	if (!ActorClass)
	{
		return;
	}

	const AActor* ActorCDO = Cast<AActor>(ActorClass->GetDefaultObject());
	Test.TestNotNull(FString::Printf(TEXT("%s has an actor CDO"), ActorLabel), ActorCDO);
	if (ActorCDO)
	{
		Test.TestNotNull(
			FString::Printf(TEXT("%s owns a default combat feedback component"), ActorLabel),
			ActorCDO->FindComponentByClass(FeedbackComponentClass));
	}
}
}

bool FPRCombatFeedbackContractTest::RunTest(const FString& Parameters)
{
	UEnum* ReactionStrengthEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRHitReactionStrength"));
	TestNotNull(TEXT("EPRHitReactionStrength is registered"), ReactionStrengthEnum);
	if (ReactionStrengthEnum)
	{
		for (const TCHAR* ValueName : { TEXT("None"), TEXT("Light"), TEXT("Heavy") })
		{
			TestTrue(
				*FString::Printf(TEXT("EPRHitReactionStrength exposes %s"), ValueName),
				ReactionStrengthEnum->GetValueByNameString(ValueName) != INDEX_NONE);
		}
	}

	UEnum* FeedbackPolicyEnum = FindObject<UEnum>(nullptr, TEXT("/Script/ProjectA.EPRCombatFeedbackPolicy"));
	TestNotNull(TEXT("EPRCombatFeedbackPolicy is registered"), FeedbackPolicyEnum);
	if (FeedbackPolicyEnum)
	{
		for (const TCHAR* ValueName : { TEXT("None"), TEXT("TargetOnly"), TEXT("TargetAndSource") })
		{
			TestTrue(
				*FString::Printf(TEXT("EPRCombatFeedbackPolicy exposes %s"), ValueName),
				FeedbackPolicyEnum->GetValueByNameString(ValueName) != INDEX_NONE);
		}
	}

	UScriptStruct* ReactionDefinition = RequireStruct(*this, TEXT("PRHitReactionDefinition"));
	if (ReactionDefinition)
	{
		FEnumProperty* Strength = RequireProperty<FEnumProperty>(*this, ReactionDefinition, TEXT("Strength"));
		RequireProperty<FFloatProperty>(*this, ReactionDefinition, TEXT("DurationSeconds"));
		if (Strength && ReactionStrengthEnum)
		{
			TestEqual(TEXT("FPRHitReactionDefinition Strength uses EPRHitReactionStrength"), Strength->GetEnum(), ReactionStrengthEnum);
		}
	}

	UScriptStruct* DamageRequest = RequireStruct(*this, TEXT("PRDamageRequest"));
	if (DamageRequest)
	{
		RequireProperty<FFloatProperty>(*this, DamageRequest, TEXT("BaseDamage"));
		FStructProperty* DamageType = RequireProperty<FStructProperty>(*this, DamageRequest, TEXT("DamageType"));
		FStructProperty* RequestHitResult = RequireProperty<FStructProperty>(*this, DamageRequest, TEXT("HitResult"));
		FStructProperty* RequestHitReaction = RequireProperty<FStructProperty>(*this, DamageRequest, TEXT("HitReaction"));
		FEnumProperty* FeedbackPolicy = RequireProperty<FEnumProperty>(*this, DamageRequest, TEXT("FeedbackPolicy"));

		if (DamageType)
		{
			TestEqual<const UScriptStruct*>(
				TEXT("FPRDamageRequest DamageType uses FGameplayTag"),
				DamageType->Struct.Get(),
				FGameplayTag::StaticStruct());
		}
		if (RequestHitResult)
		{
			TestEqual<const UScriptStruct*>(
				TEXT("FPRDamageRequest HitResult uses FHitResult"),
				RequestHitResult->Struct.Get(),
				FHitResult::StaticStruct());
		}
		if (RequestHitReaction && ReactionDefinition)
		{
			TestEqual<const UScriptStruct*>(
				TEXT("FPRDamageRequest HitReaction uses FPRHitReactionDefinition"),
				RequestHitReaction->Struct.Get(),
				ReactionDefinition);
		}
		if (FeedbackPolicy && FeedbackPolicyEnum)
		{
			TestEqual(TEXT("FPRDamageRequest FeedbackPolicy uses EPRCombatFeedbackPolicy"), FeedbackPolicy->GetEnum(), FeedbackPolicyEnum);
		}

		FStructOnScope RequestDefaults(DamageRequest);
		if (RequestHitResult && RequestHitResult->Struct == FHitResult::StaticStruct())
		{
			TestFalse(
				TEXT("A default damage request allows an invalid non-blocking hit result"),
				RequestHitResult->ContainerPtrToValuePtr<FHitResult>(RequestDefaults.GetStructMemory())->IsValidBlockingHit());
		}
		if (FeedbackPolicy && FeedbackPolicyEnum)
		{
			const uint64 DefaultPolicyValue = FeedbackPolicy->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(
				FeedbackPolicy->ContainerPtrToValuePtr<void>(RequestDefaults.GetStructMemory()));
			const int64 NonePolicyValue = FeedbackPolicyEnum->GetValueByNameString(TEXT("None"));
			TestTrue(TEXT("EPRCombatFeedbackPolicy defines None"), NonePolicyValue != INDEX_NONE);
			if (NonePolicyValue != INDEX_NONE)
			{
				TestEqual(
					TEXT("A default damage request preserves legacy no-feedback behavior"),
					DefaultPolicyValue,
					static_cast<uint64>(NonePolicyValue));
			}
		}
	}

	UScriptStruct* ResolvedDamage = RequireStruct(*this, TEXT("PRResolvedDamage"));
	if (ResolvedDamage)
	{
		RequireProperty<FFloatProperty>(*this, ResolvedDamage, TEXT("ShieldDamage"));
		RequireProperty<FFloatProperty>(*this, ResolvedDamage, TEXT("HealthDamage"));
		RequireProperty<FBoolProperty>(*this, ResolvedDamage, TEXT("bShieldBroken"));
		RequireProperty<FBoolProperty>(*this, ResolvedDamage, TEXT("bLethal"));
	}

	UScriptStruct* HitConfirmation = RequireStruct(*this, TEXT("PRHitConfirmation"));
	if (HitConfirmation)
	{
		RequireProperty<FFloatProperty>(*this, HitConfirmation, TEXT("ShieldDamage"));
		RequireProperty<FFloatProperty>(*this, HitConfirmation, TEXT("HealthDamage"));
		RequireProperty<FBoolProperty>(*this, HitConfirmation, TEXT("bShieldBroken"));
		RequireProperty<FBoolProperty>(*this, HitConfirmation, TEXT("bLethal"));
	}

	UClass* HitStaggerEffectClass = RequireClass(
		*this,
		TEXT("UPRHitStaggerGameplayEffect"),
		TEXT("/Script/ProjectA.PRHitStaggerGameplayEffect"));
	if (HitStaggerEffectClass)
	{
		TestTrue(
			TEXT("UPRHitStaggerGameplayEffect derives from UGameplayEffect"),
			HitStaggerEffectClass->IsChildOf(UGameplayEffect::StaticClass()));
	}

	UClass* FeedbackComponentClass = RequireClass(
		*this,
		TEXT("UPRCombatFeedbackComponent"),
		TEXT("/Script/ProjectA.PRCombatFeedbackComponent"));
	if (FeedbackComponentClass)
	{
		TestTrue(
			TEXT("UPRCombatFeedbackComponent derives from UActorComponent"),
			FeedbackComponentClass->IsChildOf(UActorComponent::StaticClass()));
	}
	TestActorHasFeedbackComponent(
		*this,
		TEXT("APRCharacter"),
		TEXT("/Script/ProjectA.PRCharacter"),
		FeedbackComponentClass);
	TestActorHasFeedbackComponent(
		*this,
		TEXT("APREnemyCharacter"),
		TEXT("/Script/ProjectA.PREnemyCharacter"),
		FeedbackComponentClass);

	UClass* GameplayCueNotifyBase = RequireClass(
		*this,
		TEXT("UPRGameplayCueNotifyBase"),
		TEXT("/Script/ProjectA.PRGameplayCueNotifyBase"));
	if (GameplayCueNotifyBase)
	{
		UClass* StaticCueNotifyClass = RequireClass(
			*this,
			TEXT("UGameplayCueNotify_Static"),
			TEXT("/Script/GameplayAbilities.GameplayCueNotify_Static"));
		if (StaticCueNotifyClass)
		{
			TestTrue(
				TEXT("UPRGameplayCueNotifyBase derives from UGameplayCueNotify_Static"),
				GameplayCueNotifyBase->IsChildOf(StaticCueNotifyClass));
		}
	}

	const FGameplayTag HitStaggeredTag = RequireGameplayTag(*this, TEXT("State.HitStaggered"));
	for (const TCHAR* TagName :
		{
			TEXT("GameplayCue.Combat.Impact.Shield"),
			TEXT("GameplayCue.Combat.Impact.ShieldBreak"),
			TEXT("GameplayCue.Combat.Impact.Health"),
			TEXT("GameplayCue.Combat.Impact.Lethal"),
		})
	{
		RequireGameplayTag(*this, TagName);
	}

	FGameplayTagContainer RegisteredTags;
	UGameplayTagsManager::Get().RequestAllGameplayTags(RegisteredTags, false);
	TArray<FGameplayTag> RegisteredTagArray;
	RegisteredTags.GetGameplayTagArray(RegisteredTagArray);
	int32 ProjectRiftStatusCueCount = 0;
	for (const FGameplayTag Tag : RegisteredTagArray)
	{
		const FString TagName = Tag.ToString();
		if (TagName.StartsWith(TEXT("GameplayCue.Combat.Status."))
			|| TagName.StartsWith(TEXT("GameplayCue.ProjectRift.Status.")))
		{
			++ProjectRiftStatusCueCount;
		}
	}
	TestTrue(
		TEXT("At least three ProjectRift status GameplayCue tags are registered"),
		ProjectRiftStatusCueCount >= 3);

	UClass* WeaponDataClass = RequireClass(
		*this,
		TEXT("UPRWeaponDataAsset"),
		TEXT("/Script/ProjectA.PRWeaponDataAsset"));
	if (WeaponDataClass)
	{
		FStructProperty* WeaponHitReaction = RequireProperty<FStructProperty>(
			*this,
			WeaponDataClass,
			TEXT("HitReaction"));
		if (WeaponHitReaction && ReactionDefinition)
		{
			TestEqual<const UScriptStruct*>(
				TEXT("UPRWeaponDataAsset HitReaction uses FPRHitReactionDefinition"),
				WeaponHitReaction->Struct.Get(),
				ReactionDefinition);
		}
	}

	UClass* GameplayAbilityClass = RequireClass(
		*this,
		TEXT("UPRGameplayAbility"),
		TEXT("/Script/ProjectA.PRGameplayAbility"));
	if (GameplayAbilityClass && HitStaggeredTag.IsValid())
	{
		const UGameplayAbility* GameplayAbility = Cast<UGameplayAbility>(GameplayAbilityClass->GetDefaultObject());
		const FStructProperty* BlockedTagsProperty = FindFProperty<FStructProperty>(
			GameplayAbilityClass,
			TEXT("ActivationBlockedTags"));
		const FGameplayTagContainer* BlockedTags = GameplayAbility && BlockedTagsProperty
			? BlockedTagsProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(GameplayAbility)
			: nullptr;
		TestTrue(
			TEXT("UPRGameplayAbility blocks activation while hit-staggered"),
			BlockedTags && BlockedTags->HasTagExact(HitStaggeredTag));
	}

	UClass* CombatEffectLibraryClass = RequireClass(
		*this,
		TEXT("UPRCombatEffectLibrary"),
		TEXT("/Script/ProjectA.PRCombatEffectLibrary"));
	RequireFunction(*this, CombatEffectLibraryClass, TEXT("ApplyDamageRequestToTarget"));
	RequireFunction(*this, CombatEffectLibraryClass, TEXT("DispatchResolvedDamageFeedback"));
	UFunction* ApplyDamageRequestFunction = CombatEffectLibraryClass
		? CombatEffectLibraryClass->FindFunctionByName(TEXT("ApplyDamageRequestToTarget"))
		: nullptr;
	UFunction* DispatchResolvedDamageFunction = CombatEffectLibraryClass
		? CombatEffectLibraryClass->FindFunctionByName(TEXT("DispatchResolvedDamageFeedback"))
		: nullptr;
	TestTrue(
		TEXT("ApplyDamageRequestToTarget is BlueprintAuthorityOnly"),
		ApplyDamageRequestFunction
			&& ApplyDamageRequestFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	TestTrue(
		TEXT("DispatchResolvedDamageFeedback is BlueprintAuthorityOnly"),
		DispatchResolvedDamageFunction
			&& DispatchResolvedDamageFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

	UClass* WeaponHUDClass = RequireClass(
		*this,
		TEXT("UPRWeaponHUDWidget"),
		TEXT("/Script/ProjectA.PRWeaponHUDWidget"));
	RequireFunction(*this, WeaponHUDClass, TEXT("PushHitConfirmation"));
	RequireFunction(*this, WeaponHUDClass, TEXT("PushLocalHitFeedback"));

	TArray<FString> CueNotifyPaths;
	GConfig->GetArray(
		TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"),
		TEXT("GameplayCueNotifyPaths"),
		CueNotifyPaths,
		GGameIni);
	const FString ExpectedCueNotifyPath = TEXT("/Game/ProjectRift/GameplayCues");
	TestTrue(
		TEXT("GameplayCueNotifyPaths contains the exact normalized ProjectRift cue directory"),
		CueNotifyPaths.ContainsByPredicate([&ExpectedCueNotifyPath](const FString& Path)
		{
			return NormalizeCueNotifyPath(Path).Equals(ExpectedCueNotifyPath, ESearchCase::CaseSensitive);
		}));

	FString ProjectVersion;
	TestTrue(
		TEXT("ProjectVersion is configured"),
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectVersion"),
			ProjectVersion,
			GGameIni));
	TestEqual(TEXT("ProjectVersion is v0.8.6"), ProjectVersion, FString(TEXT("0.8.6")));

	return true;
}

#endif
