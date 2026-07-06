#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagsManager.h"
#include "GameplayTask.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASSetupTest, "ProjectRift.GAS.Setup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRGASSetupTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("AbilitySystemComponent class is available"), UAbilitySystemComponent::StaticClass());
	TestNotNull(TEXT("GameplayAbility class is available"), UGameplayAbility::StaticClass());
	TestNotNull(TEXT("GameplayTask class is available"), UGameplayTask::StaticClass());

	const FGameplayTag PrimaryInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Primary"), false);
	const FGameplayTag AssaultRoleTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Ability.Role.Assault"), false);
	const FGameplayTag DeadStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Dead"), false);
	const FGameplayTag DamageDataTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage"), false);
	const FGameplayTag SkillQCooldownTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Cooldown.Skill.Q"), false);

	TestTrue(TEXT("Input ability tags are configured"), PrimaryInputTag.IsValid());
	TestTrue(TEXT("Role ability tags are configured"), AssaultRoleTag.IsValid());
	TestTrue(TEXT("State tags are configured"), DeadStateTag.IsValid());
	TestTrue(TEXT("Damage SetByCaller tag is configured"), DamageDataTag.IsValid());
	TestTrue(TEXT("Cooldown tags are configured"), SkillQCooldownTag.IsValid());

	return true;
}

#endif
