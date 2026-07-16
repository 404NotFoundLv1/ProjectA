#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultBlast.h"
#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_AssaultShield.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatFeedbackTypes.h"
#include "Abilities/PRGameplayAbility.h"
#include "Abilities/PRTemporaryShieldGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRAssetManager.h"
#include "Engine/StaticMeshActor.h"
#include "Enemies/PREnemyCharacter.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleComponent.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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

void TickAssaultModuleTestWorld(UWorld* World, const float DurationSeconds)
{
	const float StepSeconds = 0.05f;
	for (float Elapsed = 0.0f; World && Elapsed < DurationSeconds; Elapsed += StepSeconds)
	{
		++GFrameCounter;
		World->Tick(ELevelTick::LEVELTICK_All, FMath::Min(StepSeconds, DurationSeconds - Elapsed));
	}
}

int32 CountActiveEffectsOfClass(const UAbilitySystemComponent* AbilitySystemComponent, const UClass* EffectClass)
{
	if (!AbilitySystemComponent || !EffectClass)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FActiveGameplayEffectHandle& Handle : AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
		if (ActiveEffect && ActiveEffect->Spec.Def && ActiveEffect->Spec.Def->IsA(EffectClass))
		{
			++Count;
		}
	}
	return Count;
}

void ConfigureBlockingTestCube(AStaticMeshActor* CubeActor)
{
	if (!CubeActor)
	{
		return;
	}

	UStaticMeshComponent* MeshComponent = CubeActor->GetStaticMeshComponent();
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!MeshComponent || !CubeMesh)
	{
		return;
	}

	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetStaticMesh(CubeMesh);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CubeActor->SetActorScale3D(FVector(0.40f, 4.0f, 4.0f));
}
}

bool FPRGASAssaultModuleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Assault charge derives from ProjectRift ability"), UGA_AssaultCharge::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Assault blast derives from ProjectRift ability"), UGA_AssaultBlast::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Assault shield derives from ProjectRift ability"), UGA_AssaultShield::StaticClass()->IsChildOf(UPRGameplayAbility::StaticClass()));
	TestTrue(TEXT("Temporary shield derives from GameplayEffect"), UPRTemporaryShieldGameplayEffect::StaticClass()->IsChildOf(UGameplayEffect::StaticClass()));
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TestNotNull(TEXT("ProjectRift asset manager is available for role module definitions"), AssetManager);
	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> ModuleDefinitions;
	TestTrue(TEXT("Assault role module catalog loads"), AssetManager && AssetManager->LoadRoleCatalog(Roles, ModuleDefinitions));
	auto FindModule = [&ModuleDefinitions](const FName ModuleId)
	{
		return ModuleDefinitions.FindByPredicate([ModuleId](const UPRRoleModuleDataAsset* Module)
		{
			return Module && Module->ModuleId == ModuleId;
		});
	};
	const UPRRoleModuleDataAsset* const* ChargeModule = FindModule(TEXT("Ability.Module.Assault.Charge"));
	const UPRRoleModuleDataAsset* const* BlastModule = FindModule(TEXT("Ability.Module.Assault.Blast"));
	const UPRRoleModuleDataAsset* const* ShieldModule = FindModule(TEXT("Ability.Module.Assault.Shield"));
	TestNotNull(TEXT("Charge module definition exists"), ChargeModule ? *ChargeModule : nullptr);
	TestNotNull(TEXT("Blast module definition exists"), BlastModule ? *BlastModule : nullptr);
	TestNotNull(TEXT("Shield module definition exists"), ShieldModule ? *ShieldModule : nullptr);
	if (ChargeModule && BlastModule && ShieldModule)
	{
		TestEqual(TEXT("Charge module definition cost is fifteen"), (*ChargeModule)->EnergyCost, 15.0f);
		TestEqual(TEXT("Charge module definition cooldown is five seconds"), (*ChargeModule)->CooldownSeconds, 5.0f);
		TestEqual(TEXT("Blast module definition cost is twenty-five"), (*BlastModule)->EnergyCost, 25.0f);
		TestEqual(TEXT("Blast module definition cooldown is eight seconds"), (*BlastModule)->CooldownSeconds, 8.0f);
		TestEqual(TEXT("Shield module definition cost is thirty"), (*ShieldModule)->EnergyCost, 30.0f);
		TestEqual(TEXT("Shield module definition cooldown is fourteen seconds"), (*ShieldModule)->CooldownSeconds, 14.0f);
		TestNotNull(TEXT("Charge module has an ability class"), (*ChargeModule)->GameplayAbilityClass.Get());
		TestNotNull(TEXT("Blast module has an ability class"), (*BlastModule)->GameplayAbilityClass.Get());
		TestNotNull(TEXT("Shield module has an ability class"), (*ShieldModule)->GameplayAbilityClass.Get());
		TestTrue(TEXT("Charge asset class derives from the native charge behavior"), (*ChargeModule)->GameplayAbilityClass->IsChildOf(UGA_AssaultCharge::StaticClass()));
		TestTrue(TEXT("Blast asset class derives from the native blast behavior"), (*BlastModule)->GameplayAbilityClass->IsChildOf(UGA_AssaultBlast::StaticClass()));
		TestTrue(TEXT("Shield asset class derives from the native shield behavior"), (*ShieldModule)->GameplayAbilityClass->IsChildOf(UGA_AssaultShield::StaticClass()));
	}
	const FStructProperty* BlastHitReactionProperty = FindFProperty<FStructProperty>(UGA_AssaultBlast::StaticClass(), TEXT("HitReaction"));
	TestNotNull(TEXT("Assault blast declares its HitReaction definition"), BlastHitReactionProperty);
	if (BlastHitReactionProperty)
	{
		TestEqual(
			TEXT("Assault blast HitReaction uses FPRHitReactionDefinition"),
			BlastHitReactionProperty->Struct.Get(),
			FPRHitReactionDefinition::StaticStruct());
		if (BlastHitReactionProperty->Struct == FPRHitReactionDefinition::StaticStruct())
		{
			const FPRHitReactionDefinition* BlastHitReaction =
				BlastHitReactionProperty->ContainerPtrToValuePtr<FPRHitReactionDefinition>(UGA_AssaultBlast::StaticClass()->GetDefaultObject());
			TestEqual(TEXT("Assault blast defaults to a Heavy presentation reaction"), BlastHitReaction->Strength, EPRHitReactionStrength::Heavy);
			TestTrue(
				TEXT("Assault blast reaction duration is zero because stun owns control"),
				FMath::IsNearlyZero(BlastHitReaction->DurationSeconds));
		}
	}

	UClass* CharacterClass = APRCharacter::StaticClass();
	TestNotNull(TEXT("APRCharacter exposes selected role grant entry point"), CharacterClass->FindFunctionByName(TEXT("GrantSelectedRoleModuleAbilities")));
	TestNotNull(TEXT("APRCharacter exposes role grant state"), CharacterClass->FindFunctionByName(TEXT("AreRoleModuleAbilitiesGranted")));

	UPRRoleDataAsset* AssaultRoleDefinition = AssetManager ? AssetManager->LoadRoleSync(TEXT("Ability.Role.Assault")) : nullptr;
	TestNotNull(TEXT("Assault role data is available for restored-loadout initialization"), AssaultRoleDefinition);
	if (AssaultRoleDefinition)
	{
		const FPRRoleLoadout RestoredLoadout = AssaultRoleDefinition->DefaultLoadout;
		FPRRoleLoadout ReorderedDefaultLoadout = RestoredLoadout;
		if (ReorderedDefaultLoadout.Entries.Num() >= 2)
		{
			Swap(ReorderedDefaultLoadout.Entries[0], ReorderedDefaultLoadout.Entries[1]);
		}
		TestTrue(TEXT("Reordered default is structurally valid"), ReorderedDefaultLoadout.IsStructurallyValid());
		AssaultRoleDefinition->DefaultLoadout = ReorderedDefaultLoadout;

		FTestWorldWrapper RestoredLoadoutWorldWrapper;
		TestTrue(TEXT("Restored-loadout world is created"), RestoredLoadoutWorldWrapper.CreateTestWorld(EWorldType::Game));
		TestTrue(TEXT("Restored-loadout world begins play"), RestoredLoadoutWorldWrapper.BeginPlayInTestWorld());
		UWorld* RestoredLoadoutWorld = RestoredLoadoutWorldWrapper.GetTestWorld();
		APRPlayerState* RestoredPlayerState = RestoredLoadoutWorld ? RestoredLoadoutWorld->SpawnActor<APRPlayerState>() : nullptr;
		UPRRoleComponent* RestoredRoles = RestoredPlayerState ? RestoredPlayerState->GetRoleComponent() : nullptr;
		APRCharacter* RestoredPawn = RestoredLoadoutWorld ? RestoredLoadoutWorld->SpawnActor<APRCharacter>() : nullptr;
		TestNotNull(TEXT("Restored-loadout PlayerState exists"), RestoredPlayerState);
		TestNotNull(TEXT("Restored-loadout role component exists"), RestoredRoles);
		TestNotNull(TEXT("Restored-loadout pawn exists"), RestoredPawn);
		if (RestoredPlayerState && RestoredRoles && RestoredPawn)
		{
			RestoredRoles->ApplyProfileRoleState(
				TEXT("Ability.Role.Assault"),
				{ TEXT("Ability.Role.Assault") },
				RestoredLoadout,
				{ TEXT("Ability.Module.Assault.Charge"), TEXT("Ability.Module.Assault.Blast"), TEXT("Ability.Module.Assault.Shield") });
			TestTrue(TEXT("Restored role loadout is valid before possession"), RestoredRoles->IsLoadoutValid(TEXT("Ability.Role.Assault"), RestoredRoles->GetCurrentLoadout()));
			TestTrue(TEXT("Possession initializes the pawn with a restored role loadout"), PossessTestCharacterForAssaultModuleTest(RestoredLoadoutWorld, RestoredPlayerState, RestoredPawn));
			TestEqual(
				TEXT("Pawn initialization preserves a valid restored loadout instead of re-provisioning the role default"),
				RestoredRoles->GetCurrentLoadout().Entries,
				RestoredLoadout.Entries);
		}

		AssaultRoleDefinition->DefaultLoadout = RestoredLoadout;
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
	TStrongObjectPtr<APRCharacter> Target{ World->SpawnActor<APRCharacter>(FVector(220.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	TStrongObjectPtr<APREnemyCharacter> ChargeTarget{ World->SpawnActor<APREnemyCharacter>(FVector(450.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	TStrongObjectPtr<APREnemyCharacter> BlastTarget{ World->SpawnActor<APREnemyCharacter>(FVector(220.0f, 80.0f, 0.0f), FRotator::ZeroRotator) };

	TestNotNull(TEXT("Attacker PlayerState spawned"), AttackerPlayerState.Get());
	TestNotNull(TEXT("Attacker Character spawned"), Attacker.Get());
	TestNotNull(TEXT("Target PlayerState spawned"), TargetPlayerState.Get());
	TestNotNull(TEXT("Target Character spawned"), Target.Get());
	TestNotNull(TEXT("Charge enemy target spawned"), ChargeTarget.Get());
	TestNotNull(TEXT("Blast enemy target spawned"), BlastTarget.Get());
	if (!AttackerPlayerState || !Attacker || !TargetPlayerState || !Target || !ChargeTarget || !BlastTarget)
	{
		return false;
	}

	AttackerPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Attacker initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, AttackerPlayerState.Get(), Attacker.Get()));
	TestTrue(TEXT("Target initializes ASC"), PossessTestCharacterForAssaultModuleTest(World, TargetPlayerState.Get(), Target.Get()));
	UPRRoleComponent* AttackerRoles = AttackerPlayerState->GetRoleComponent();
	TestNotNull(TEXT("Attacker role component exists after possession"), AttackerRoles);
	if (AttackerRoles)
	{
		TestTrue(TEXT("Attacker current role loadout is structurally valid"), AttackerRoles->GetCurrentLoadout().IsStructurallyValid());
		const bool bLoadoutValidBeforeEnsure = AttackerRoles->IsLoadoutValid(
			AttackerPlayerState->GetSelectedRoleModule(),
			AttackerRoles->GetCurrentLoadout());
		if (!bLoadoutValidBeforeEnsure)
		{
			TestTrue(TEXT("Automatic assault default loadout provisions after possession"), AttackerRoles->EnsureDefaultLoadoutForSelectedRole());
		}
		TestTrue(
			TEXT("Attacker selected role loadout is catalog-valid after possession"),
			AttackerRoles->IsLoadoutValid(AttackerPlayerState->GetSelectedRoleModule(), AttackerRoles->GetCurrentLoadout()));
	}

	UPRAbilitySystemComponent* AttackerASC = Attacker->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* AttackerAttributes = AttackerPlayerState->GetAttributeSet();
	UPRAttributeSet* TargetAttributes = TargetPlayerState->GetAttributeSet();
	UPRAttributeSet* ChargeTargetAttributes = ChargeTarget->GetAttributeSet();
	UPRAttributeSet* BlastTargetAttributes = BlastTarget->GetAttributeSet();
	UPRCombatFeedbackComponent* BlastTargetFeedback = BlastTarget->FindComponentByClass<UPRCombatFeedbackComponent>();
	TestNotNull(TEXT("Attacker ASC exists"), AttackerASC);
	TestNotNull(TEXT("Attacker AttributeSet exists"), AttackerAttributes);
	TestNotNull(TEXT("Target AttributeSet exists"), TargetAttributes);
	TestNotNull(TEXT("Charge enemy AttributeSet exists"), ChargeTargetAttributes);
	TestNotNull(TEXT("Blast enemy AttributeSet exists"), BlastTargetAttributes);
	TestNotNull(TEXT("Blast enemy feedback component exists"), BlastTargetFeedback);
	if (!AttackerASC || !AttackerAttributes || !TargetAttributes || !ChargeTargetAttributes || !BlastTargetAttributes || !BlastTargetFeedback)
	{
		return false;
	}

	TestTrue(TEXT("Assault role grants Q/E/R abilities during ASC initialization"), Attacker->AreRoleModuleAbilitiesGranted());
	auto FindRoleModuleSpec = [AttackerASC](const UObject* ModuleSource, const FGameplayTag& InputTag) -> const FGameplayAbilitySpec*
	{
		for (const FGameplayAbilitySpec& Spec : AttackerASC->GetActivatableAbilities())
		{
			if (Spec.SourceObject.Get() == ModuleSource && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				return &Spec;
			}
		}
		return nullptr;
	};
	const FGameplayAbilitySpec* ChargeSpec = ChargeModule
		? FindRoleModuleSpec(*ChargeModule, ProjectRiftGameplayTags::Input_Ability_Skill_Q) : nullptr;
	const FGameplayAbilitySpec* BlastSpec = BlastModule
		? FindRoleModuleSpec(*BlastModule, ProjectRiftGameplayTags::Input_Ability_Skill_E) : nullptr;
	const FGameplayAbilitySpec* ShieldSpec = ShieldModule
		? FindRoleModuleSpec(*ShieldModule, ProjectRiftGameplayTags::Input_Ability_Skill_R) : nullptr;
	TestNotNull(TEXT("Q charge ability is granted"), ChargeSpec);
	TestNotNull(TEXT("E blast ability is granted"), BlastSpec);
	TestNotNull(TEXT("R shield ability is granted"), ShieldSpec);
	if (ChargeSpec && BlastSpec && ShieldSpec && ChargeModule && BlastModule && ShieldModule)
	{
		TestTrue(TEXT("Q spec source is its module definition"), ChargeSpec->SourceObject.Get() == *ChargeModule);
		TestTrue(TEXT("E spec source is its module definition"), BlastSpec->SourceObject.Get() == *BlastModule);
		TestTrue(TEXT("R spec source is its module definition"), ShieldSpec->SourceObject.Get() == *ShieldModule);
	}
	const UGA_AssaultShield* ShieldAbilityDefault = ShieldSpec ? Cast<UGA_AssaultShield>(ShieldSpec->Ability) : nullptr;
	const FClassProperty* ShieldEffectClassProperty = FindFProperty<FClassProperty>(UGA_AssaultShield::StaticClass(), TEXT("ShieldEffectClass"));
	const UClass* RuntimeShieldEffectClass = (ShieldAbilityDefault && ShieldEffectClassProperty)
		? Cast<UClass>(ShieldEffectClassProperty->GetPropertyValue_InContainer(ShieldAbilityDefault))
		: nullptr;
	TestNotNull(TEXT("R shield ability exposes its configured temporary shield effect class"), RuntimeShieldEffectClass);
	TestTrue(
		TEXT("R shield effect class remains a temporary shield effect"),
		RuntimeShieldEffectClass && RuntimeShieldEffectClass->IsChildOf(UPRTemporaryShieldGameplayEffect::StaticClass()));

	AttackerAttributes->SetEnergy(100.0f);
	const FVector ChargeStart = Attacker->GetActorLocation();
	Attacker->DoSkillQ();
	TestTrue(TEXT("Q charge moves the attacker forward"), Attacker->GetActorLocation().X > ChargeStart.X + 250.0f);
	TestEqual(TEXT("Q charge consumes energy"), AttackerAttributes->GetEnergy(), 85.0f);
	TestEqual(TEXT("Q charge slows an enemy through its ability definition"), ChargeTargetAttributes->GetMoveSpeed(), 252.0f);
	TestTrue(
		TEXT("Q charge grants the slow status tag"),
		ChargeTarget->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));
	TestEqual(
		TEXT("Q path and endpoint overlap apply one slow instance to a duplicated target"),
		ChargeTarget->GetProjectRiftAbilitySystemComponent()->GetActiveEffects(FGameplayEffectQuery()).Num(),
		1);
	TestEqual(TEXT("Q excludes friendly players from slow"), TargetAttributes->GetMoveSpeed(), 600.0f);
	TestFalse(
		TEXT("Q excludes friendly players from the slow state"),
		Target->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));
	TestTrue(
		TEXT("Q excludes friendly players from knockback"),
		Target->GetCharacterMovement()->Velocity.IsNearlyZero());

	TStrongObjectPtr<APREnemyCharacter> WallBlockedChargeTarget{
		World->SpawnActor<APREnemyCharacter>(FVector(420.0f, -1000.0f, 0.0f), FRotator::ZeroRotator) };
	AStaticMeshActor* ChargeWall = World->SpawnActor<AStaticMeshActor>(FVector(180.0f, -1000.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Charge wall-block target spawned"), WallBlockedChargeTarget.Get());
	TestNotNull(TEXT("Charge wall spawned"), ChargeWall);
	ConfigureBlockingTestCube(ChargeWall);
	if (WallBlockedChargeTarget && ChargeWall)
	{
		UPRAttributeSet* WallBlockedChargeAttributes = WallBlockedChargeTarget->GetAttributeSet();
		TestNotNull(TEXT("Charge wall-block target attributes exist"), WallBlockedChargeAttributes);
		if (WallBlockedChargeAttributes)
		{
			const float WallTargetBaseSpeed = WallBlockedChargeAttributes->GetMoveSpeed();
			FGameplayTagContainer ChargeCooldownTags;
			ChargeCooldownTags.AddTag(ProjectRiftGameplayTags::Cooldown_Skill_Q);
			AttackerASC->RemoveActiveEffectsWithGrantedTags(ChargeCooldownTags);
			Attacker->SetActorLocation(FVector(0.0f, -1000.0f, 0.0f));
			Attacker->SetActorRotation(FRotator::ZeroRotator);
			AttackerAttributes->SetEnergy(100.0f);
			Attacker->DoSkillQ();
			TestTrue(TEXT("Q charge stops before a blocking world wall"), Attacker->GetActorLocation().X < 250.0f);
			TestEqual(TEXT("Q wall prevents slow on targets beyond the stopped endpoint"), WallBlockedChargeAttributes->GetMoveSpeed(), WallTargetBaseSpeed);
			TestFalse(
				TEXT("Q wall prevents slow state on targets beyond the stopped endpoint"),
				WallBlockedChargeTarget->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::Status_Debuff_Slowed));
		}
		ChargeWall->SetActorEnableCollision(false);
		ChargeWall->Destroy();
	}

	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetShield(50.0f);
	BlastTargetAttributes->SetHealth(50.0f);
	Attacker->SetActorLocation(FVector::ZeroVector);
	const int32 BlastFeedbackCountBefore = BlastTargetFeedback->FeedbackDispatchCount;
	Attacker->DoSkillE();
	TestEqual(TEXT("E blast rejects player friendly fire"), TargetAttributes->GetShield(), 50.0f);
	TestEqual(TEXT("E blast preserves friendly player health"), TargetAttributes->GetHealth(), 100.0f);
	TestEqual(TEXT("E blast damages a GAS enemy through unified execution"), BlastTargetAttributes->GetHealth(), 22.5f);
	TestTrue(
		TEXT("E blast stuns the enemy"),
		BlastTarget->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
	TestEqual(
		TEXT("E blast dispatches exactly one resolved target feedback event"),
		BlastTargetFeedback->FeedbackDispatchCount,
		BlastFeedbackCountBefore + 1);
	TestTrue(TEXT("E blast feedback reports actual post-execution health damage"), FMath::IsNearlyEqual(BlastTargetFeedback->LastResolvedDamage.HealthDamage, 27.5f, 0.01f));
	const FEnumProperty* ResolvedHitReactionProperty = FindFProperty<FEnumProperty>(
		FPRResolvedDamage::StaticStruct(),
		TEXT("HitReactionStrength"));
	TestNotNull(
		TEXT("Resolved damage carries the requested hit reaction for zero-duration presentation"),
		ResolvedHitReactionProperty);
	if (ResolvedHitReactionProperty)
	{
		const void* ResolvedHitReactionValue = ResolvedHitReactionProperty->ContainerPtrToValuePtr<void>(
			&BlastTargetFeedback->LastResolvedDamage);
		const uint64 EncodedHitReaction = ResolvedHitReactionProperty->GetUnderlyingProperty()
			->GetUnsignedIntPropertyValue(ResolvedHitReactionValue);
		TestEqual(
			TEXT("E blast runtime feedback preserves its Heavy presentation reaction"),
			EncodedHitReaction,
			static_cast<uint64>(EPRHitReactionStrength::Heavy));
	}
	TestEqual(
		TEXT("E blast Heavy/zero definition does not create an extra stagger effect"),
		BlastTargetFeedback->GetActiveStaggerStrength(),
		EPRHitReactionStrength::None);
	TestFalse(
		TEXT("E blast control remains exclusively State.Stunned"),
		BlastTarget->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
	TestEqual(
		TEXT("E blast damage preserves the attacker as original instigator"),
		BlastTargetFeedback->LastDamageEffectContext.GetOriginalInstigator(),
		static_cast<AActor*>(Attacker.Get()));
	const FHitResult* BlastHitResult = BlastTargetFeedback->LastDamageEffectContext.GetHitResult();
	TestNotNull(TEXT("E blast synthesizes a target-position hit result"), BlastHitResult);
	if (BlastHitResult)
	{
		TestEqual(TEXT("E blast synthetic hit preserves the enemy target"), BlastHitResult->GetActor(), static_cast<AActor*>(BlastTarget.Get()));
		TestTrue(
			TEXT("E blast synthetic hit uses the target position"),
			BlastHitResult->ImpactPoint.Equals(BlastTarget->GetActorLocation(), 1.0f));
	}
	TestEqual(TEXT("E blast consumes energy"), AttackerAttributes->GetEnergy(), 60.0f);

	TStrongObjectPtr<APREnemyCharacter> LineOfSightBlockedBlastTarget{
		World->SpawnActor<APREnemyCharacter>(FVector(220.0f, -1800.0f, 0.0f), FRotator::ZeroRotator) };
	AStaticMeshActor* BlastWall = World->SpawnActor<AStaticMeshActor>(FVector(110.0f, -1800.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("Blast line-of-sight target spawned"), LineOfSightBlockedBlastTarget.Get());
	TestNotNull(TEXT("Blast line-of-sight wall spawned"), BlastWall);
	ConfigureBlockingTestCube(BlastWall);
	if (LineOfSightBlockedBlastTarget && BlastWall)
	{
		UPRAttributeSet* LineOfSightBlockedAttributes = LineOfSightBlockedBlastTarget->GetAttributeSet();
		TestNotNull(TEXT("Blast line-of-sight target attributes exist"), LineOfSightBlockedAttributes);
		if (LineOfSightBlockedAttributes)
		{
			LineOfSightBlockedAttributes->SetHealth(50.0f);
			LineOfSightBlockedAttributes->SetShield(0.0f);
			FGameplayTagContainer BlastCooldownTags;
			BlastCooldownTags.AddTag(ProjectRiftGameplayTags::Cooldown_Skill_E);
			AttackerASC->RemoveActiveEffectsWithGrantedTags(BlastCooldownTags);
			Attacker->SetActorLocation(FVector(0.0f, -1800.0f, 0.0f));
			Attacker->SetActorRotation(FRotator::ZeroRotator);
			AttackerAttributes->SetEnergy(100.0f);
			Attacker->DoSkillE();
			TestEqual(TEXT("E blast world line-of-sight block preserves health"), LineOfSightBlockedAttributes->GetHealth(), 50.0f);
			TestFalse(
				TEXT("E blast world line-of-sight block prevents stun"),
				LineOfSightBlockedBlastTarget->GetProjectRiftAbilitySystemComponent()->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned));
		}
		BlastWall->SetActorEnableCollision(false);
		BlastWall->Destroy();
	}

	TStrongObjectPtr<APREnemyCharacter> DuplicateBlastTarget{
		World->SpawnActor<APREnemyCharacter>(FVector(220.0f, -2400.0f, 0.0f), FRotator::ZeroRotator) };
	TestNotNull(TEXT("Duplicate-overlap blast target spawned"), DuplicateBlastTarget.Get());
	if (DuplicateBlastTarget)
	{
		UPRAttributeSet* DuplicateBlastAttributes = DuplicateBlastTarget->GetAttributeSet();
		UPRCombatFeedbackComponent* DuplicateBlastFeedback = DuplicateBlastTarget->FindComponentByClass<UPRCombatFeedbackComponent>();
		TestNotNull(TEXT("Duplicate-overlap blast target attributes exist"), DuplicateBlastAttributes);
		TestNotNull(TEXT("Duplicate-overlap blast target feedback exists"), DuplicateBlastFeedback);
		USphereComponent* DuplicateOverlapComponent = NewObject<USphereComponent>(DuplicateBlastTarget.Get(), TEXT("DuplicateBlastOverlap"));
		TestNotNull(TEXT("Duplicate-overlap component is created"), DuplicateOverlapComponent);
		if (DuplicateBlastAttributes && DuplicateBlastFeedback && DuplicateOverlapComponent)
		{
			DuplicateBlastTarget->AddInstanceComponent(DuplicateOverlapComponent);
			DuplicateOverlapComponent->SetupAttachment(DuplicateBlastTarget->GetRootComponent());
			DuplicateOverlapComponent->InitSphereRadius(72.0f);
			DuplicateOverlapComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			DuplicateOverlapComponent->SetCollisionObjectType(ECC_Pawn);
			DuplicateOverlapComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
			DuplicateOverlapComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			DuplicateOverlapComponent->RegisterComponent();
			DuplicateBlastAttributes->SetHealth(50.0f);
			DuplicateBlastAttributes->SetShield(0.0f);
			const int32 DuplicateFeedbackBefore = DuplicateBlastFeedback->FeedbackDispatchCount;
			FGameplayTagContainer BlastCooldownTags;
			BlastCooldownTags.AddTag(ProjectRiftGameplayTags::Cooldown_Skill_E);
			AttackerASC->RemoveActiveEffectsWithGrantedTags(BlastCooldownTags);
			Attacker->SetActorLocation(FVector(0.0f, -2400.0f, 0.0f));
			Attacker->SetActorRotation(FRotator::ZeroRotator);
			AttackerAttributes->SetEnergy(100.0f);
			Attacker->DoSkillE();
			TestTrue(TEXT("E blast applies its damage once when an enemy has duplicate overlap components"), FMath::IsNearlyEqual(DuplicateBlastAttributes->GetHealth(), 22.5f, 0.01f));
			TestEqual(TEXT("E blast dispatches one feedback event when an enemy has duplicate overlap components"), DuplicateBlastFeedback->FeedbackDispatchCount, DuplicateFeedbackBefore + 1);
		}
	}

	const float EnergyBeforeBlastCooldownRetry = AttackerAttributes->GetEnergy();
	Attacker->DoSkillE();
	TestEqual(TEXT("E blast cooldown blocks immediate second damage"), BlastTargetAttributes->GetHealth(), 22.5f);
	TestEqual(TEXT("E blast cooldown does not consume energy again"), AttackerAttributes->GetEnergy(), EnergyBeforeBlastCooldownRetry);

	AttackerAttributes->SetEnergy(60.0f);
	AttackerAttributes->SetShield(10.0f);
	Attacker->DoSkillR();
	TestEqual(TEXT("R tactical shield grants shield"), AttackerAttributes->GetShield(), 40.0f);
	TestEqual(TEXT("R tactical shield raises maximum shield by thirty"), AttackerAttributes->GetMaxShield(), 80.0f);
	TestEqual(TEXT("R tactical shield consumes energy"), AttackerAttributes->GetEnergy(), 30.0f);
	Attacker->DoSkillR();
	TestEqual(TEXT("R cooldown blocks immediate reactivation without a second energy debit"), AttackerAttributes->GetEnergy(), 30.0f);
	FGameplayTagContainer ShieldCooldownTags;
	ShieldCooldownTags.AddTag(ProjectRiftGameplayTags::Cooldown_Skill_R);
	AttackerASC->RemoveActiveEffectsWithGrantedTags(ShieldCooldownTags);
	AttackerAttributes->SetEnergy(100.0f);
	AttackerAttributes->SetShield(50.0f);
	Attacker->DoSkillR();
	TestEqual(TEXT("R refresh keeps one temporary maximum shield bonus"), AttackerAttributes->GetMaxShield(), 80.0f);
	TestEqual(TEXT("R refresh does not double-increase current shield"), AttackerAttributes->GetShield(), 80.0f);
	TestEqual(TEXT("R refresh maintains one active temporary shield effect"), CountActiveEffectsOfClass(AttackerASC, RuntimeShieldEffectClass), 1);
	TickAssaultModuleTestWorld(World, 5.1f);
	TestEqual(TEXT("R refresh extends the temporary maximum shield duration"), AttackerAttributes->GetMaxShield(), 80.0f);
	TestEqual(TEXT("R refresh still has one active temporary shield effect before expiry"), CountActiveEffectsOfClass(AttackerASC, RuntimeShieldEffectClass), 1);
	TickAssaultModuleTestWorld(World, 1.1f);
	TestEqual(TEXT("R temporary maximum shield expires after six seconds"), AttackerAttributes->GetMaxShield(), 50.0f);
	TestEqual(TEXT("R temporary shield effect is removed on expiry"), CountActiveEffectsOfClass(AttackerASC, RuntimeShieldEffectClass), 0);
	TestTrue(TEXT("R expiry clamps current shield to its restored maximum"), AttackerAttributes->GetShield() <= AttackerAttributes->GetMaxShield());

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
	EnergyStarvedAttributes->SetEnergy(90.0f);
	TickAssaultModuleTestWorld(World, 0.05f);
	TestTrue(TEXT("Role energy regeneration does not execute immediately when applied"), FMath::IsNearlyEqual(EnergyStarvedAttributes->GetEnergy(), 90.0f, 0.05f));
	TickAssaultModuleTestWorld(World, 1.05f);
	TestTrue(TEXT("Assault role regenerates four energy per second"), FMath::IsNearlyEqual(EnergyStarvedAttributes->GetEnergy(), 94.0f, 0.15f));
	UPRAbilitySystemComponent* EnergyStarvedASC = EnergyStarvedAttacker->GetProjectRiftAbilitySystemComponent();
	TestNotNull(TEXT("Energy regeneration attacker ASC exists"), EnergyStarvedASC);
	if (EnergyStarvedASC)
	{
		EnergyStarvedASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Downed);
		const float EnergyAtDowned = EnergyStarvedAttributes->GetEnergy();
		TickAssaultModuleTestWorld(World, 1.05f);
		TestTrue(TEXT("Downed state pauses role energy regeneration"), FMath::IsNearlyEqual(EnergyStarvedAttributes->GetEnergy(), EnergyAtDowned, 0.05f));
		EnergyStarvedASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_Downed);
		EnergyStarvedAttributes->SetEnergy(90.0f);
		EnergyStarvedASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_Dead);
		const float EnergyAtDead = EnergyStarvedAttributes->GetEnergy();
		TickAssaultModuleTestWorld(World, 1.05f);
		TestTrue(TEXT("Dead state pauses role energy regeneration"), FMath::IsNearlyEqual(EnergyStarvedAttributes->GetEnergy(), EnergyAtDead, 0.05f));
		EnergyStarvedASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_Dead);
		EnergyStarvedAttributes->SetEnergy(100.0f);
		EnergyStarvedAttributes->SetShield(10.0f);
		EnergyStarvedAttributes->SetCooldownReduction(1.0f);
		EnergyStarvedAttacker->DoSkillR();
		const float EnergyAfterCappedCooldownActivation = EnergyStarvedAttributes->GetEnergy();
		TickAssaultModuleTestWorld(World, 2.9f);
		EnergyStarvedAttacker->DoSkillR();
		TestTrue(
			TEXT("Cooldown reduction clamps at eighty percent, allowing R again after 2.8 seconds"),
			EnergyStarvedAttributes->GetEnergy() < EnergyAfterCappedCooldownActivation);
	}

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

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	TestTrue(TEXT("State.Downed tag is configured"), DownedStateTag.IsValid());
	if (DownedStateTag.IsValid())
	{
		DownedASC->AddLooseGameplayTag(DownedStateTag);
	}

	DownedTargetAttributes->SetShield(50.0f);
	DownedAttacker->DoSkillE();
	TestEqual(TEXT("Assault skills cannot activate while downed"), DownedTargetAttributes->GetShield(), 50.0f);

	// Candidate grants must not replace a live role loadout until role energy
	// regeneration can also be refreshed.  This forces the non-matching grant
	// path, then makes regeneration invalid to exercise the transaction failure.
	if (AttackerRoles && AttackerASC && AssaultRoleDefinition)
	{
		FGameplayAbilitySpecHandle OriginalHandle;
		UObject* OriginalSourceObject = nullptr;
		for (const FGameplayAbilitySpec& Spec : AttackerASC->GetActivatableAbilities())
		{
			if (Spec.SourceObject.Get() && Spec.SourceObject.Get()->IsA<UPRRoleModuleDataAsset>())
			{
				OriginalHandle = Spec.Handle;
				OriginalSourceObject = Spec.SourceObject.Get();
				break;
			}
		}
		FGameplayAbilitySpec* MutableOriginalSpec = OriginalHandle.IsValid() ? AttackerASC->FindAbilitySpecFromHandle(OriginalHandle) : nullptr;
		TestNotNull(TEXT("A live role ability spec exists before a failed grant transaction"), MutableOriginalSpec);
		if (MutableOriginalSpec)
		{
			const float OriginalEnergyRegen = AssaultRoleDefinition->EnergyRegenPerSecond;
			MutableOriginalSpec->SourceObject = NewObject<UPRRoleModuleDataAsset>(AttackerRoles);
			AssaultRoleDefinition->EnergyRegenPerSecond = 0.0f;
			TestFalse(TEXT("Invalid role energy regeneration rejects the candidate grant transaction"), AttackerRoles->RefreshGrantedAbilities());
			TestNotNull(
				TEXT("A rejected candidate grant transaction preserves the original live ability spec"),
				AttackerASC->FindAbilitySpecFromHandle(OriginalHandle));
			if (FGameplayAbilitySpec* PreservedSpec = AttackerASC->FindAbilitySpecFromHandle(OriginalHandle))
			{
				PreservedSpec->SourceObject = OriginalSourceObject;
			}
			AssaultRoleDefinition->EnergyRegenPerSecond = OriginalEnergyRegen;
		}
	}

	return true;
}

#endif
