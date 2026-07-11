#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Core/PRGameplayTags.h"
#include "GameplayTask.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRGASSetupTest, "ProjectRift.GAS.Setup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRGASSetupTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("AbilitySystemComponent class is available"), UAbilitySystemComponent::StaticClass());
	TestNotNull(TEXT("GameplayAbility class is available"), UGameplayAbility::StaticClass());
	TestNotNull(TEXT("GameplayTask class is available"), UGameplayTask::StaticClass());

	const FGameplayTag PrimaryInputTag = ProjectRiftGameplayTags::Input_Ability_Primary;
	const FGameplayTag AssaultRoleTag = ProjectRiftGameplayTags::Ability_Role_Assault;
	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	const FGameplayTag DamageDataTag = ProjectRiftGameplayTags::Data_Damage;
	const FGameplayTag SkillQCooldownTag = ProjectRiftGameplayTags::Cooldown_Skill_Q;

	TestTrue(TEXT("Input ability tags are configured"), PrimaryInputTag.IsValid());
	TestTrue(TEXT("Role ability tags are configured"), AssaultRoleTag.IsValid());
	TestTrue(TEXT("State tags are configured"), DeadStateTag.IsValid());
	TestTrue(TEXT("Damage SetByCaller tag is configured"), DamageDataTag.IsValid());
	TestTrue(TEXT("Cooldown tags are configured"), SkillQCooldownTag.IsValid());

	return true;
}

#endif
