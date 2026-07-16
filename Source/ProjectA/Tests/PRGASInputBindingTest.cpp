#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_AssaultShield.h"
#include "Abilities/GA_PrimaryAttack.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Characters/PRCharacter.h"
#include "Core/PRGameplayTags.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/PlayerController.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleModuleDataAsset.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/PRWeaponComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASInputBindingTest, "ProjectRift.GAS.InputBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool PossessTestCharacterForInputBindingTest(UWorld* World, APRPlayerState* PlayerState, APRCharacter* Character)
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

bool SpecHasInputTag(const FGameplayAbilitySpec* Spec, const FGameplayTag InputTag)
{
	return Spec && InputTag.IsValid() && Spec->GetDynamicSpecSourceTags().HasTagExact(InputTag);
}

FGameplayAbilitySpec* FindRoleModuleSpecByInputTag(
	UPRAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTag InputTag,
	const FName ExpectedModuleId = NAME_None)
{
	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const UPRRoleModuleDataAsset* Module = Cast<UPRRoleModuleDataAsset>(Spec.SourceObject.Get());
		if (Module && SpecHasInputTag(&Spec, InputTag)
			&& (ExpectedModuleId.IsNone() || Module->ModuleId == ExpectedModuleId))
		{
			return &Spec;
		}
	}
	return nullptr;
}
}

bool FPRGASInputBindingTest::RunTest(const FString& Parameters)
{
	UClass* AbilitySystemClass = UPRAbilitySystemComponent::StaticClass();
	TestNotNull(TEXT("ASC exposes input pressed by tag"), AbilitySystemClass->FindFunctionByName(TEXT("AbilityInputTagPressed")));
	TestNotNull(TEXT("ASC exposes input released by tag"), AbilitySystemClass->FindFunctionByName(TEXT("AbilityInputTagReleased")));
	TestNotNull(TEXT("ASC exposes direct activation by input tag"), AbilitySystemClass->FindFunctionByName(TEXT("TryActivateAbilityByInputTag")));

	UClass* CharacterClass = APRCharacter::StaticClass();
	TestNotNull(TEXT("APRCharacter exposes primary release input"), CharacterClass->FindFunctionByName(TEXT("DoPrimaryAttackReleased")));
	TestNotNull(TEXT("APRCharacter exposes Q release input"), CharacterClass->FindFunctionByName(TEXT("DoSkillQReleased")));
	TestNotNull(TEXT("APRCharacter exposes E release input"), CharacterClass->FindFunctionByName(TEXT("DoSkillEReleased")));
	TestNotNull(TEXT("APRCharacter exposes R release input"), CharacterClass->FindFunctionByName(TEXT("DoSkillRReleased")));

	const FGameplayTag PrimaryInputTag = ProjectRiftGameplayTags::Input_Ability_Primary;
	const FGameplayTag QInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_Q;
	const FGameplayTag EInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_E;
	const FGameplayTag RInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_R;
	TestTrue(TEXT("Primary input tag is configured"), PrimaryInputTag.IsValid());
	TestTrue(TEXT("Q input tag is configured"), QInputTag.IsValid());
	TestTrue(TEXT("E input tag is configured"), EInputTag.IsValid());
	TestTrue(TEXT("R input tag is configured"), RInputTag.IsValid());

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
	TStrongObjectPtr<APREnemyCharacter> Target{ World->SpawnActor<APREnemyCharacter>(FVector(160.0f, 0.0f, 0.0f), FRotator::ZeroRotator) };
	if (!AttackerPlayerState || !Attacker || !Target)
	{
		return false;
	}

	AttackerPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Attacker initializes ASC"), PossessTestCharacterForInputBindingTest(World, AttackerPlayerState.Get(), Attacker.Get()));
	FString WeaponDiagnostic;
	TestTrue(TEXT("Primary input attacker equips the starter rifle"), AttackerPlayerState->GetWeaponComponent()->EnsureStarterWeapon(TEXT("TestRifle"), WeaponDiagnostic));

	UPRAbilitySystemComponent* AttackerASC = Attacker->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* AttackerAttributes = AttackerPlayerState->GetAttributeSet();
	UPRAttributeSet* TargetAttributes = Target->GetAttributeSet();
	TestNotNull(TEXT("Attacker ASC exists"), AttackerASC);
	TestNotNull(TEXT("Attacker attributes exist"), AttackerAttributes);
	TestNotNull(TEXT("Target attributes exist"), TargetAttributes);
	if (!AttackerASC || !AttackerAttributes || !TargetAttributes)
	{
		return false;
	}

	FGameplayAbilitySpec* PrimarySpec = AttackerASC->FindAbilitySpecFromClass(UGA_PrimaryAttack::StaticClass());
	FGameplayAbilitySpec* ChargeSpec = FindRoleModuleSpecByInputTag(AttackerASC, QInputTag, TEXT("Ability.Module.Assault.Charge"));
	TestTrue(TEXT("Primary ability has primary input tag"), SpecHasInputTag(PrimarySpec, PrimaryInputTag));
	TestTrue(TEXT("Data-driven Q module ability has Q input tag"), SpecHasInputTag(ChargeSpec, QInputTag));
	TestTrue(TEXT("Data-driven Q module spec retains its module definition as source"), ChargeSpec && Cast<UPRRoleModuleDataAsset>(ChargeSpec->SourceObject.Get()) != nullptr);

	TargetAttributes->SetMaxHealth(100.0f);
	TargetAttributes->SetHealth(100.0f);
	TargetAttributes->SetMaxShield(50.0f);
	TargetAttributes->SetShield(50.0f);
	TestTrue(TEXT("Primary input tag press is handled"), AttackerASC->AbilityInputTagPressed(PrimaryInputTag));
	TestTrue(TEXT("Primary input tag activates AttackPower-scaled rifle attack"), FMath::IsNearlyEqual(TargetAttributes->GetShield(), 36.8f, 0.01f));
	TestTrue(TEXT("Primary spec sees pressed input"), PrimarySpec && PrimarySpec->InputPressed);
	TestTrue(TEXT("Primary input tag release is handled"), AttackerASC->AbilityInputTagReleased(PrimaryInputTag));
	TestFalse(TEXT("Primary spec sees released input"), PrimarySpec && PrimarySpec->InputPressed);

	TestTrue(TEXT("Q input tag press is handled"), AttackerASC->AbilityInputTagPressed(QInputTag));
	TestTrue(TEXT("Q spec sees pressed input"), ChargeSpec && ChargeSpec->InputPressed);
	TestTrue(TEXT("Q input tag release is handled"), AttackerASC->AbilityInputTagReleased(QInputTag));
	TestFalse(TEXT("Q spec sees released input"), ChargeSpec && ChargeSpec->InputPressed);

	TStrongObjectPtr<APRPlayerState> ReplacementPlayerState{ World->SpawnActor<APRPlayerState>() };
	TStrongObjectPtr<APRCharacter> ReplacementCharacter{ World->SpawnActor<APRCharacter>(FVector(0.0f, 500.0f, 0.0f), FRotator::ZeroRotator) };
	if (!ReplacementPlayerState || !ReplacementCharacter)
	{
		return false;
	}

	ReplacementPlayerState->SetSelectedRoleModule(TEXT("Ability.Role.Assault"));
	TestTrue(TEXT("Replacement character initializes ASC"), PossessTestCharacterForInputBindingTest(World, ReplacementPlayerState.Get(), ReplacementCharacter.Get()));

	UPRAbilitySystemComponent* ReplacementASC = ReplacementCharacter->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* ReplacementAttributes = ReplacementPlayerState->GetAttributeSet();
	TestNotNull(TEXT("Replacement ASC exists"), ReplacementASC);
	TestNotNull(TEXT("Replacement attributes exist"), ReplacementAttributes);
	if (!ReplacementASC || !ReplacementAttributes)
	{
		return false;
	}

	FGameplayAbilitySpec* ReplacementChargeSpec = FindRoleModuleSpecByInputTag(
		ReplacementASC,
		QInputTag,
		TEXT("Ability.Module.Assault.Charge"));
	TestNotNull(TEXT("Replacement state has a data-driven Q module spec"), ReplacementChargeSpec);
	if (ReplacementChargeSpec)
	{
		ReplacementASC->ClearAbility(ReplacementChargeSpec->Handle);
	}

	FGameplayAbilitySpec* ReplacementShieldSpec = FindRoleModuleSpecByInputTag(
		ReplacementASC,
		RInputTag,
		TEXT("Ability.Module.Assault.Shield"));
	UPRRoleModuleDataAsset* ReplacementShieldModuleSource = ReplacementShieldSpec
		? Cast<UPRRoleModuleDataAsset>(ReplacementShieldSpec->SourceObject.Get())
		: nullptr;
	TestNotNull(TEXT("Replacement state has a data-driven shield module spec"), ReplacementShieldSpec);
	TestNotNull(TEXT("Replacement shield retains its matching module source"), ReplacementShieldModuleSource);

	FGameplayAbilitySpec ReplacementQSpec(ReplacementShieldModuleSource ? ReplacementShieldModuleSource->GameplayAbilityClass : nullptr, 1, INDEX_NONE, ReplacementShieldModuleSource);
	ReplacementQSpec.GetDynamicSpecSourceTags().AddTag(QInputTag);
	ReplacementASC->GiveAbility(ReplacementQSpec);
	TestNotNull(TEXT("Replacement Q ability keeps its matching role-module source object"), ReplacementShieldModuleSource);
	TestNotNull(TEXT("Replacement Q spec is found by its data-driven source and Q tag"), FindRoleModuleSpecByInputTag(ReplacementASC, QInputTag, TEXT("Ability.Module.Assault.Shield")));

	ReplacementAttributes->SetEnergy(100.0f);
	ReplacementAttributes->SetShield(10.0f);
	const FVector ReplacementStart = ReplacementCharacter->GetActorLocation();
	ReplacementCharacter->DoSkillQ();
	TestEqual(TEXT("Q input activates replacement ability with same tag"), ReplacementAttributes->GetShield(), 40.0f);
	TestEqual(TEXT("Replacement Q ability consumes replacement ability energy cost"), ReplacementAttributes->GetEnergy(), 70.0f);
	TestEqual(TEXT("Q replacement does not run removed charge movement"), ReplacementCharacter->GetActorLocation(), ReplacementStart);

	return true;
}

#endif
