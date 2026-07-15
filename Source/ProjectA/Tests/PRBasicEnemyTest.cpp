#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AIController.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Blueprint/UserWidget.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Weapons/PRWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRBasicEnemyTest, "ProjectRift.Rift.BasicEnemy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
UClass* LoadBasicEnemyRuntimeClass(const TCHAR* ClassName, UClass* ExpectedBaseClass)
{
	const FString ClassPath = FString::Printf(TEXT("/Script/ProjectA.%s"), ClassName);
	return StaticLoadClass(ExpectedBaseClass, nullptr, *ClassPath);
}

bool TestBasicEnemyReplicatedProperty(FAutomationTestBase& Test, UClass* Class, const FName PropertyName)
{
	FProperty* Property = Class ? Class->FindPropertyByName(PropertyName) : nullptr;
	Test.TestNotNull(FString::Printf(TEXT("%s exposes %s"), *GetNameSafe(Class), *PropertyName.ToString()), Property);
	if (!Property)
	{
		return false;
	}

	Test.TestTrue(
		FString::Printf(TEXT("%s is replicated"), *PropertyName.ToString()),
		Property->HasAnyPropertyFlags(CPF_Net));
	return true;
}

bool PossessBasicEnemyTestCharacter(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
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

bool CallBasicEnemyBoolFunctionNoParams(UObject* Target, const FName FunctionName, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallBasicEnemyFloatFunctionNoParams(UObject* Target, const FName FunctionName, float& OutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FFloatProperty* ReturnValue = FindFProperty<FFloatProperty>(Function, TEXT("ReturnValue")))
	{
		OutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallBasicEnemyBoolFunctionWithActorParam(UObject* Target, const FName FunctionName, const FName ParamName, AActor* ParamValue, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	if (FObjectProperty* ObjectParam = FindFProperty<FObjectProperty>(Function, ParamName))
	{
		ObjectParam->SetObjectPropertyValue_InContainer(ParamsMemory, ParamValue);
	}

	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

bool CallBasicEnemyBoolFunctionWithDamageParams(UObject* Target, const FName FunctionName, const float DamageAmount, AController* DamageInstigator, bool& bOutResult)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	if (FFloatProperty* DamageParam = FindFProperty<FFloatProperty>(Function, TEXT("DamageAmount")))
	{
		DamageParam->SetPropertyValue_InContainer(ParamsMemory, DamageAmount);
	}
	if (FObjectProperty* InstigatorParam = FindFProperty<FObjectProperty>(Function, TEXT("DamageInstigator")))
	{
		InstigatorParam->SetObjectPropertyValue_InContainer(ParamsMemory, DamageInstigator);
	}

	Target->ProcessEvent(Function, ParamsMemory);

	if (FBoolProperty* ReturnValue = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")))
	{
		bOutResult = ReturnValue->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	return false;
}

APawn* CallBasicEnemyPawnFunctionNoParams(UObject* Target, const FName FunctionName)
{
	UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
	if (!Function)
	{
		return nullptr;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();
	Target->ProcessEvent(Function, ParamsMemory);

	if (FObjectProperty* ReturnValue = FindFProperty<FObjectProperty>(Function, TEXT("ReturnValue")))
	{
		return Cast<APawn>(ReturnValue->GetObjectPropertyValue_InContainer(ParamsMemory));
	}

	return nullptr;
}

int32 CountEnemyTestPickups(UWorld* World)
{
	int32 PickupCount = 0;
	if (!World)
	{
		return PickupCount;
	}

	for (TActorIterator<APRPickupActor> PickupIt(World); PickupIt; ++PickupIt)
	{
		++PickupCount;
	}

	return PickupCount;
}

int32 GetBasicEnemyIntPropertyValue(const UObject* Target, const FName PropertyName)
{
	const FIntProperty* Property = Target ? FindFProperty<FIntProperty>(Target->GetClass(), PropertyName) : nullptr;
	return Property ? Property->GetPropertyValue_InContainer(Target) : 0;
}
}

bool FPRBasicEnemyTest::RunTest(const FString& Parameters)
{
	UClass* EnemyClass = LoadBasicEnemyRuntimeClass(TEXT("PREnemyCharacter"), ACharacter::StaticClass());
	UClass* EnemyAIControllerClass = LoadBasicEnemyRuntimeClass(TEXT("PREnemyAIController"), AAIController::StaticClass());
	UClass* EnemyHealthBarWidgetClass = LoadBasicEnemyRuntimeClass(TEXT("PREnemyHealthBarWidget"), UUserWidget::StaticClass());
	UClass* EnemyBlueprintClass = StaticLoadClass(ACharacter::StaticClass(), nullptr, TEXT("/Game/ProjectRift/Blueprints/Enemies/BP_PREnemyCharacter.BP_PREnemyCharacter_C"));
	UClass* SpawnManagerClass = LoadBasicEnemyRuntimeClass(TEXT("PRSpawnManager"), AActor::StaticClass());
	UClass* RiftGameModeClass = APRRiftGameMode::StaticClass();

	TestNotNull(TEXT("APREnemyCharacter runtime class exists"), EnemyClass);
	TestNotNull(TEXT("APREnemyAIController runtime class exists"), EnemyAIControllerClass);
	TestNotNull(TEXT("UPREnemyHealthBarWidget runtime class exists"), EnemyHealthBarWidgetClass);
	TestNotNull(TEXT("BP_PREnemyCharacter asset exists for placed/spawned enemies"), EnemyBlueprintClass);
	if (!EnemyClass || !EnemyAIControllerClass)
	{
		return false;
	}

	TestTrue(TEXT("APREnemyCharacter derives from ACharacter"), EnemyClass->IsChildOf(ACharacter::StaticClass()));
	TestTrue(TEXT("APREnemyAIController derives from AAIController"), EnemyAIControllerClass->IsChildOf(AAIController::StaticClass()));
	TestTrue(TEXT("UPREnemyHealthBarWidget derives from UUserWidget"), EnemyHealthBarWidgetClass && EnemyHealthBarWidgetClass->IsChildOf(UUserWidget::StaticClass()));
	TestTrue(TEXT("BP_PREnemyCharacter derives from APREnemyCharacter"), EnemyBlueprintClass && EnemyBlueprintClass->IsChildOf(EnemyClass));

	TestNotNull(TEXT("Enemy exposes ApplyEnemyDamage"), EnemyClass->FindFunctionByName(TEXT("ApplyEnemyDamage")));
	TestNotNull(TEXT("Enemy exposes TryMeleeAttack"), EnemyClass->FindFunctionByName(TEXT("TryMeleeAttack")));
	TestNotNull(TEXT("Enemy exposes IsDead"), EnemyClass->FindFunctionByName(TEXT("IsDead")));
	TestNotNull(TEXT("Enemy exposes GetHealth"), EnemyClass->FindFunctionByName(TEXT("GetHealth")));
	TestNotNull(TEXT("Enemy exposes GetMaxHealth"), EnemyClass->FindFunctionByName(TEXT("GetMaxHealth")));
	TestNotNull(TEXT("Enemy exposes GetHealthPercent"), EnemyClass->FindFunctionByName(TEXT("GetHealthPercent")));
	TestNotNull(TEXT("Enemy exposes GetAttackDamage"), EnemyClass->FindFunctionByName(TEXT("GetAttackDamage")));
	TestNotNull(TEXT("Enemy exposes SpawnDeathLoot"), EnemyClass->FindFunctionByName(TEXT("SpawnDeathLoot")));
	TestNotNull(TEXT("Enemy AI exposes RefreshTarget"), EnemyAIControllerClass->FindFunctionByName(TEXT("RefreshTarget")));
	TestNotNull(TEXT("Enemy AI exposes GetCurrentTarget"), EnemyAIControllerClass->FindFunctionByName(TEXT("GetCurrentTarget")));
	TestNotNull(TEXT("Rift GameMode exposes RegisterEnemyKilled for enemy death notifications"), RiftGameModeClass->FindFunctionByName(TEXT("RegisterEnemyKilled")));

	TestTrue(TEXT("Enemy implements IAbilitySystemInterface"), EnemyClass->ImplementsInterface(UAbilitySystemInterface::StaticClass()));
	const FProperty* LegacyHealthProperty = EnemyClass->FindPropertyByName(TEXT("Health"));
	const FProperty* LegacyDeadProperty = EnemyClass->FindPropertyByName(TEXT("bDead"));
	TestFalse(TEXT("Enemy Health is no longer a replicated authority"), LegacyHealthProperty && LegacyHealthProperty->HasAnyPropertyFlags(CPF_Net));
	TestFalse(TEXT("Enemy bDead is no longer a replicated authority"), LegacyDeadProperty && LegacyDeadProperty->HasAnyPropertyFlags(CPF_Net));

	const ACharacter* EnemyDefaults = Cast<ACharacter>(EnemyClass->GetDefaultObject());
	TestNotNull(TEXT("Enemy has a default object"), EnemyDefaults);
	if (EnemyDefaults)
	{
		TestTrue(TEXT("Enemy actor replicates to clients"), EnemyDefaults->GetIsReplicated());
		TestTrue(TEXT("Enemy movement replicates to clients"), EnemyDefaults->IsReplicatingMovement());
		TestTrue(TEXT("Enemy uses APREnemyAIController by default"), EnemyDefaults->AIControllerClass && EnemyDefaults->AIControllerClass->IsChildOf(EnemyAIControllerClass));
		TestEqual(TEXT("Enemy auto-possesses spawned AI"), static_cast<uint8>(EnemyDefaults->AutoPossessAI), static_cast<uint8>(EAutoPossessAI::PlacedInWorldOrSpawned));
		TestTrue(TEXT("Enemy default mesh is a visible humanoid skeletal mesh"), EnemyDefaults->GetMesh() && EnemyDefaults->GetMesh()->GetSkeletalMeshAsset() != nullptr && EnemyDefaults->GetMesh()->IsVisible());

		const UWidgetComponent* HealthBarWidget = Cast<UWidgetComponent>(const_cast<ACharacter*>(EnemyDefaults)->GetDefaultSubobjectByName(TEXT("EnemyHealthBarWidget")));
		TestNotNull(TEXT("Enemy owns a head health bar widget component"), HealthBarWidget);
		if (HealthBarWidget)
		{
			TestTrue(TEXT("Enemy health bar uses the native health bar widget"), HealthBarWidget->GetWidgetClass() && HealthBarWidget->GetWidgetClass()->IsChildOf(EnemyHealthBarWidgetClass));
			TestTrue(TEXT("Enemy health bar sits above the character"), HealthBarWidget->GetRelativeLocation().Z >= 180.0f);
			TestTrue(TEXT("Enemy health bar is wide enough to read"), HealthBarWidget->GetDrawSize().X >= 180.0f);
		}
	}

	if (SpawnManagerClass)
	{
		const AActor* SpawnManagerDefaults = Cast<AActor>(SpawnManagerClass->GetDefaultObject());
		const FClassProperty* SpawnedEnemyProperty = FindFProperty<FClassProperty>(SpawnManagerClass, TEXT("SpawnedEnemyClass"));
		const UClass* DefaultSpawnedEnemyClass = SpawnedEnemyProperty && SpawnManagerDefaults
			? Cast<UClass>(SpawnedEnemyProperty->GetObjectPropertyValue_InContainer(SpawnManagerDefaults))
			: nullptr;
		TestTrue(
			TEXT("Rift spawn manager defaults to the basic enemy class"),
			DefaultSpawnedEnemyClass && DefaultSpawnedEnemyClass->IsChildOf(EnemyClass));
		TestTrue(TEXT("Rift spawn manager defaults to the enemy blueprint asset"), DefaultSpawnedEnemyClass == EnemyBlueprintClass);
	}

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Basic enemy test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	if (AWorldSettings* WorldSettings = World->GetWorldSettings())
	{
		WorldSettings->DefaultGameMode = APRRiftGameMode::StaticClass();
	}

	TestTrue(TEXT("Basic enemy test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	APRRiftGameState* RiftGameState = World->GetGameState<APRRiftGameState>();
	TestNotNull(TEXT("Basic enemy test world owns APRRiftGameState for kill tracking"), RiftGameState);
	if (!RiftGameState)
	{
		return false;
	}

	APRPlayerState* NearPlayerState = World->SpawnActor<APRPlayerState>();
	APRCharacter* NearPlayer = World->SpawnActor<APRCharacter>(FVector(180.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	APRPlayerState* FarPlayerState = World->SpawnActor<APRPlayerState>();
	APRCharacter* FarPlayer = World->SpawnActor<APRCharacter>(FVector(900.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	ACharacter* Enemy = World->SpawnActor<ACharacter>(EnemyClass, FVector::ZeroVector, FRotator::ZeroRotator);

	TestNotNull(TEXT("Near player state spawned"), NearPlayerState);
	TestNotNull(TEXT("Near player character spawned"), NearPlayer);
	TestNotNull(TEXT("Far player state spawned"), FarPlayerState);
	TestNotNull(TEXT("Far player character spawned"), FarPlayer);
	TestNotNull(TEXT("Basic enemy spawned"), Enemy);
	if (!NearPlayerState || !NearPlayer || !FarPlayerState || !FarPlayer || !Enemy)
	{
		return false;
	}

	TestTrue(TEXT("Near player initializes ASC"), PossessBasicEnemyTestCharacter(World, NearPlayerState, NearPlayer));
	TestTrue(TEXT("Far player initializes ASC"), PossessBasicEnemyTestCharacter(World, FarPlayerState, FarPlayer));

	IAbilitySystemInterface* EnemyAbilitySystemInterface = Cast<IAbilitySystemInterface>(Enemy);
	UPRAbilitySystemComponent* EnemyASC = EnemyAbilitySystemInterface
		? Cast<UPRAbilitySystemComponent>(EnemyAbilitySystemInterface->GetAbilitySystemComponent())
		: nullptr;
	const UPRAttributeSet* EnemyAttributes = EnemyASC ? EnemyASC->GetSet<UPRAttributeSet>() : nullptr;
	TestNotNull(TEXT("Enemy exposes its ASC through IAbilitySystemInterface"), EnemyASC);
	TestNotNull(TEXT("Enemy owns the shared ProjectRift AttributeSet"), EnemyAttributes);
	if (!EnemyASC || !EnemyAttributes)
	{
		return false;
	}
	TestEqual(TEXT("Enemy starts with MaxHealth 50"), EnemyAttributes->GetMaxHealth(), 50.0f);
	TestEqual(TEXT("Enemy starts with Health 50"), EnemyAttributes->GetHealth(), 50.0f);
	TestEqual(TEXT("Enemy starts without shield"), EnemyAttributes->GetShield(), 0.0f);
	TestEqual(TEXT("Enemy starts without energy"), EnemyAttributes->GetEnergy(), 0.0f);
	TestEqual(TEXT("Enemy starts with AttackPower 10"), EnemyAttributes->GetAttackPower(), 10.0f);
	TestEqual(TEXT("Enemy starts with MoveSpeed 360"), EnemyAttributes->GetMoveSpeed(), 360.0f);
	TestEqual(TEXT("Enemy starts without pollution resistance"), EnemyAttributes->GetPollutionResistance(), 0.0f);

	UClass* EnemyMeleeAbilityClass = LoadBasicEnemyRuntimeClass(TEXT("GA_EnemyMeleeAttack"), UGameplayAbility::StaticClass());
	TestNotNull(TEXT("Enemy melee GameplayAbility exists"), EnemyMeleeAbilityClass);
	const FStructProperty* EnemyMeleeHitReactionProperty = EnemyMeleeAbilityClass
		? FindFProperty<FStructProperty>(EnemyMeleeAbilityClass, TEXT("HitReaction"))
		: nullptr;
	TestNotNull(TEXT("Enemy melee ability declares its HitReaction definition"), EnemyMeleeHitReactionProperty);
	if (EnemyMeleeHitReactionProperty)
	{
		TestEqual(
			TEXT("Enemy melee HitReaction uses FPRHitReactionDefinition"),
			EnemyMeleeHitReactionProperty->Struct.Get(),
			FPRHitReactionDefinition::StaticStruct());
		if (EnemyMeleeHitReactionProperty->Struct == FPRHitReactionDefinition::StaticStruct())
		{
			const FPRHitReactionDefinition* EnemyMeleeHitReaction =
				EnemyMeleeHitReactionProperty->ContainerPtrToValuePtr<FPRHitReactionDefinition>(EnemyMeleeAbilityClass->GetDefaultObject());
			TestEqual(TEXT("Enemy melee defaults to a Heavy reaction"), EnemyMeleeHitReaction->Strength, EPRHitReactionStrength::Heavy);
			TestTrue(
				TEXT("Enemy melee defaults to a 0.30 second reaction"),
				FMath::IsNearlyEqual(EnemyMeleeHitReaction->DurationSeconds, 0.30f));
		}
	}
	TestNotNull(
		TEXT("Enemy is granted its melee GameplayAbility"),
		EnemyMeleeAbilityClass ? EnemyASC->FindAbilitySpecFromClass(EnemyMeleeAbilityClass) : nullptr);

	Enemy->SpawnDefaultController();
	AAIController* EnemyController = Cast<AAIController>(Enemy->GetController());
	TestNotNull(TEXT("Enemy is server-possessed by an AI controller"), EnemyController);
	TestTrue(TEXT("Enemy controller uses APREnemyAIController"), EnemyController && EnemyController->GetClass()->IsChildOf(EnemyAIControllerClass));
	if (!EnemyController)
	{
		return false;
	}

	bool bRefreshedTarget = false;
	TestTrue(TEXT("Can refresh enemy AI target"), CallBasicEnemyBoolFunctionNoParams(EnemyController, TEXT("RefreshTarget"), bRefreshedTarget));
	TestTrue(TEXT("Enemy AI finds a player target"), bRefreshedTarget);
	TestEqual(TEXT("Enemy AI targets the nearest living player"), CallBasicEnemyPawnFunctionNoParams(EnemyController, TEXT("GetCurrentTarget")), Cast<APawn>(NearPlayer));

	float AttackDamage = 0.0f;
	TestTrue(TEXT("Can query enemy attack damage"), CallBasicEnemyFloatFunctionNoParams(Enemy, TEXT("GetAttackDamage"), AttackDamage));
	TestTrue(TEXT("Enemy attack damage is positive"), AttackDamage > 0.0f);
	bool bAttackedFarPlayer = true;
	TestTrue(TEXT("Can attempt an out-of-range enemy melee attack"), CallBasicEnemyBoolFunctionWithActorParam(Enemy, TEXT("TryMeleeAttack"), TEXT("TargetActor"), FarPlayer, bAttackedFarPlayer));
	TestFalse(TEXT("Enemy melee rejects targets outside the ability-owned range"), bAttackedFarPlayer);

	UPRAttributeSet* NearAttributes = NearPlayerState->GetAttributeSet();
	UPRAbilitySystemComponent* NearASC = NearPlayer->GetProjectRiftAbilitySystemComponent();
	UPRCombatFeedbackComponent* NearFeedback = NearPlayer->FindComponentByClass<UPRCombatFeedbackComponent>();
	TestNotNull(TEXT("Near player attributes exist"), NearAttributes);
	TestNotNull(TEXT("Near player owns the shared feedback component"), NearFeedback);
	if (!NearAttributes || !NearASC || !NearFeedback)
	{
		return false;
	}

	NearAttributes->SetHealth(100.0f);
	NearAttributes->SetShield(50.0f);
	const float ShieldBeforeAutoAttack = NearAttributes->GetShield();
	for (int32 TickIndex = 0; TickIndex < 12; ++TickIndex)
	{
		World->Tick(ELevelTick::LEVELTICK_All, 0.1f);
		EnemyController->Tick(0.1f);
	}
	TestTrue(
		TEXT("Enemy AI automatically melee attacks its current target in range"),
		NearAttributes->GetShield() < ShieldBeforeAutoAttack - AttackDamage);
	TestTrue(
		TEXT("Enemy melee applies pollution"),
		NearPlayer->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Polluted"))));
	TestTrue(
		TEXT("Enemy melee keeps a duration-based status effect active"),
		!NearPlayer->GetProjectRiftAbilitySystemComponent()->GetActiveEffects(FGameplayEffectQuery()).IsEmpty());

	UPRCombatEffectLibrary::ClearNegativeStatusEffects(NearASC);
	const int32 MeleeFeedbackCountBefore = NearFeedback->FeedbackDispatchCount;
	bool bAttackedPlayer = false;
	TestTrue(TEXT("Can call enemy melee attack"), CallBasicEnemyBoolFunctionWithActorParam(Enemy, TEXT("TryMeleeAttack"), TEXT("TargetActor"), NearPlayer, bAttackedPlayer));
	TestTrue(TEXT("Enemy melee attack succeeds in range"), bAttackedPlayer);
	TestTrue(TEXT("Enemy melee attack damages player shield first"), NearAttributes->GetShield() < 50.0f - AttackDamage * 2.0f);
	TestEqual(TEXT("Enemy melee attack leaves player health while shield absorbs"), NearAttributes->GetHealth(), 100.0f);
	TestEqual(
		TEXT("Accepted enemy melee hit dispatches exactly one resolved target feedback event"),
		NearFeedback->FeedbackDispatchCount,
		MeleeFeedbackCountBefore + 1);
	TestEqual(TEXT("Enemy melee uses its Heavy reaction definition"), NearFeedback->GetActiveStaggerStrength(), EPRHitReactionStrength::Heavy);
	TestTrue(TEXT("Enemy melee grants State.HitStaggered"), NearASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestEqual(
		TEXT("Enemy melee damage preserves the enemy as original instigator"),
		NearFeedback->LastDamageEffectContext.GetOriginalInstigator(),
		static_cast<AActor*>(Enemy));
	const FHitResult* MeleeHitResult = NearFeedback->LastDamageEffectContext.GetHitResult();
	TestNotNull(TEXT("Enemy melee synthesizes a target-position hit result"), MeleeHitResult);
	if (MeleeHitResult)
	{
		TestEqual(TEXT("Enemy melee synthetic hit preserves the target actor"), MeleeHitResult->GetActor(), static_cast<AActor*>(NearPlayer));
		TestTrue(
			TEXT("Enemy melee synthetic hit uses the target position"),
			MeleeHitResult->ImpactPoint.Equals(NearPlayer->GetActorLocation(), 1.0f));
	}

	bool bFoundPollutionEffect = false;
	bool bPollutionPreservesEnemyInstigator = false;
	for (const FActiveGameplayEffectHandle ActiveHandle : NearASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* ActiveEffect = NearASC->GetActiveGameplayEffect(ActiveHandle);
		if (ActiveEffect && ActiveEffect->Spec.Def && ActiveEffect->Spec.Def->IsA(UPRPollutionStatusGameplayEffect::StaticClass()))
		{
			bFoundPollutionEffect = true;
			bPollutionPreservesEnemyInstigator =
				ActiveEffect->Spec.GetEffectContext().GetOriginalInstigator() == Enemy;
			break;
		}
	}
	TestTrue(TEXT("Structured enemy melee still applies pollution"), bFoundPollutionEffect);
	TestTrue(TEXT("Enemy melee pollution preserves the original enemy instigator"), bPollutionPreservesEnemyInstigator);

	float MaxHealth = 0.0f;
	float InitialHealth = 0.0f;
	TestTrue(TEXT("Can query enemy max health"), CallBasicEnemyFloatFunctionNoParams(Enemy, TEXT("GetMaxHealth"), MaxHealth));
	TestTrue(TEXT("Can query enemy health"), CallBasicEnemyFloatFunctionNoParams(Enemy, TEXT("GetHealth"), InitialHealth));
	TestTrue(TEXT("Enemy max health is positive"), MaxHealth > 0.0f);
	TestEqual(TEXT("Enemy starts at max health"), InitialHealth, MaxHealth);
	float InitialHealthPercent = -1.0f;
	TestTrue(TEXT("Can query enemy health percentage"), CallBasicEnemyFloatFunctionNoParams(Enemy, TEXT("GetHealthPercent"), InitialHealthPercent));
	TestEqual(TEXT("Enemy starts with a full health bar percentage"), InitialHealthPercent, 1.0f);

	const int32 PickupsBeforeDeath = CountEnemyTestPickups(World);
	const int32 KillCountBeforeDeath = GetBasicEnemyIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount"));
	bool bAppliedDamage = false;
	TestTrue(TEXT("Can apply lethal damage to enemy"), CallBasicEnemyBoolFunctionWithDamageParams(Enemy, TEXT("ApplyEnemyDamage"), MaxHealth + 25.0f, NearPlayer->GetController(), bAppliedDamage));
	TestTrue(TEXT("Lethal damage is accepted"), bAppliedDamage);

	float HealthAfterDeath = -1.0f;
	bool bIsDead = false;
	TestTrue(TEXT("Can query enemy health after death"), CallBasicEnemyFloatFunctionNoParams(Enemy, TEXT("GetHealth"), HealthAfterDeath));
	TestTrue(TEXT("Can query enemy death state"), CallBasicEnemyBoolFunctionNoParams(Enemy, TEXT("IsDead"), bIsDead));
	TestEqual(TEXT("Enemy health clamps to zero after lethal damage"), HealthAfterDeath, 0.0f);
	TestTrue(TEXT("Enemy enters dead state after lethal damage"), bIsDead);
	TestTrue(TEXT("Enemy death spawns at least one loot pickup"), CountEnemyTestPickups(World) > PickupsBeforeDeath);
	TestEqual(TEXT("Enemy death increments the replicated rift kill count"), GetBasicEnemyIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount")), KillCountBeforeDeath + 1);

	bool bAppliedDuplicateLethalDamage = true;
	TestTrue(
		TEXT("Can attempt duplicate lethal damage after death"),
		CallBasicEnemyBoolFunctionWithDamageParams(Enemy, TEXT("ApplyEnemyDamage"), MaxHealth + 25.0f, NearPlayer->GetController(), bAppliedDuplicateLethalDamage));
	TestFalse(TEXT("Dead enemies reject duplicate lethal damage"), bAppliedDuplicateLethalDamage);
	TestEqual(TEXT("Duplicate lethal damage does not double-count kills"), GetBasicEnemyIntPropertyValue(RiftGameState, TEXT("KilledEnemyCount")), KillCountBeforeDeath + 1);

	ACharacter* AttackTargetEnemy = World->SpawnActor<ACharacter>(EnemyClass, FVector(3160.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	APRPlayerState* AttackerState = World->SpawnActor<APRPlayerState>();
	APRCharacter* Attacker = World->SpawnActor<APRCharacter>(FVector(3000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Primary attack target enemy spawned"), AttackTargetEnemy);
	TestNotNull(TEXT("Primary attack test player state spawned"), AttackerState);
	TestNotNull(TEXT("Primary attack test player spawned"), Attacker);
	if (!AttackTargetEnemy || !AttackerState || !Attacker)
	{
		return false;
	}

	TestTrue(TEXT("Primary attack test player initializes ASC"), PossessBasicEnemyTestCharacter(World, AttackerState, Attacker));
	FString WeaponDiagnostic;
	TestTrue(TEXT("Primary attack test player equips starter rifle"), AttackerState->GetWeaponComponent()->EnsureStarterWeapon(TEXT("TestRifle"), WeaponDiagnostic));
	float EnemyHealthBeforePrimary = 0.0f;
	float EnemyHealthAfterPrimary = 0.0f;
	TestTrue(TEXT("Can query enemy health before primary attack"), CallBasicEnemyFloatFunctionNoParams(AttackTargetEnemy, TEXT("GetHealth"), EnemyHealthBeforePrimary));
	Attacker->DoPrimaryAttack();
	TestTrue(TEXT("Can query enemy health after primary attack"), CallBasicEnemyFloatFunctionNoParams(AttackTargetEnemy, TEXT("GetHealth"), EnemyHealthAfterPrimary));
	TestTrue(TEXT("Player rifle damages basic enemy through unified execution"), FMath::IsNearlyEqual(EnemyHealthAfterPrimary, EnemyHealthBeforePrimary - 13.2f, 0.01f));

	IAbilitySystemInterface* AttackTargetAbilitySystemInterface = Cast<IAbilitySystemInterface>(AttackTargetEnemy);
	UPRAbilitySystemComponent* AttackTargetASC = AttackTargetAbilitySystemInterface
		? Cast<UPRAbilitySystemComponent>(AttackTargetAbilitySystemInterface->GetAbilitySystemComponent())
		: nullptr;
	TestNotNull(TEXT("Primary target exposes its enemy ASC"), AttackTargetASC);
	if (AttackTargetASC)
	{
		AttackTargetASC->AddLooseGameplayTag(
			ProjectRiftGameplayTags::State_Dead,
			1,
			EGameplayTagReplicationState::TagOnly);
		TestFalse(TEXT("Replicated dead tag hides the enemy mesh on the receiving instance"), AttackTargetEnemy->GetMesh()->IsVisible());
		TestEqual(
			TEXT("Replicated dead tag disables enemy collision on the receiving instance"),
			AttackTargetEnemy->GetCapsuleComponent()->GetCollisionEnabled(),
			ECollisionEnabled::NoCollision);
	}

	return true;
}

#endif
