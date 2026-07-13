#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRGameplayTags.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRGameplayTagContractTest,
	"ProjectRift.GameplayTags.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRGameplayTagContractTest::RunTest(const FString& Parameters)
{
	struct FExpectedGameplayTag
	{
		FGameplayTag Tag;
		const TCHAR* Name;
	};

	const FExpectedGameplayTag ExpectedTags[] =
	{
		{ProjectRiftGameplayTags::Input_Ability_Primary, TEXT("Input.Ability.Primary")},
		{ProjectRiftGameplayTags::Input_Ability_Dodge, TEXT("Input.Ability.Dodge")},
		{ProjectRiftGameplayTags::Input_Ability_Skill_Q, TEXT("Input.Ability.Skill.Q")},
		{ProjectRiftGameplayTags::Input_Ability_Skill_E, TEXT("Input.Ability.Skill.E")},
		{ProjectRiftGameplayTags::Input_Ability_Skill_R, TEXT("Input.Ability.Skill.R")},
		{ProjectRiftGameplayTags::Ability_Role_Assault, TEXT("Ability.Role.Assault")},
		{ProjectRiftGameplayTags::Ability_Role_Engineer, TEXT("Ability.Role.Engineer")},
		{ProjectRiftGameplayTags::Ability_Role_Medic, TEXT("Ability.Role.Medic")},
		{ProjectRiftGameplayTags::Data_Damage, TEXT("Data.Damage")},
		{ProjectRiftGameplayTags::State_Dead, TEXT("State.Dead")},
		{ProjectRiftGameplayTags::State_Downed, TEXT("State.Downed")},
		{ProjectRiftGameplayTags::State_Stunned, TEXT("State.Stunned")},
		{ProjectRiftGameplayTags::Cooldown_Skill_Q, TEXT("Cooldown.Skill.Q")},
		{ProjectRiftGameplayTags::Cooldown_Skill_E, TEXT("Cooldown.Skill.E")},
		{ProjectRiftGameplayTags::Cooldown_Skill_R, TEXT("Cooldown.Skill.R")},
	};

	for (const FExpectedGameplayTag& Expected : ExpectedTags)
	{
		TestTrue(*FString::Printf(TEXT("%s is registered"), Expected.Name), Expected.Tag.IsValid());
		TestEqual(*FString::Printf(TEXT("%s keeps its contract name"), Expected.Name), Expected.Tag.ToString(), FString(Expected.Name));
		TestEqual(
			*FString::Printf(TEXT("%s resolves through GameplayTagsManager"), Expected.Name),
			Expected.Tag,
			UGameplayTagsManager::Get().RequestGameplayTag(FName(Expected.Name), false));
	}

	const FString LegacyTagConfig = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Tags/ProjectRiftGameplayTags.ini"));
	TestFalse(TEXT("Legacy GameplayTag INI is removed"), IFileManager::Get().FileExists(*LegacyTagConfig));

	FString DefaultGameContents;
	const FString DefaultGamePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
	TestTrue(TEXT("DefaultGame.ini is readable"), FFileHelper::LoadFileToString(DefaultGameContents, *DefaultGamePath));
	TestFalse(TEXT("Staging no longer references the legacy GameplayTag INI"), DefaultGameContents.Contains(TEXT("ProjectRiftGameplayTags.ini")));

	FString ProjectVersion;
	TestTrue(
		TEXT("ProjectVersion is configured"),
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), ProjectVersion, GGameIni));
	TestEqual(TEXT("ProjectVersion is v0.5.2"), ProjectVersion, FString(TEXT("0.5.2")));

	return true;
}

#endif
