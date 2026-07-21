#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Misc/ConfigCacheIni.h"
#include "Settings/PRProjectSettings.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRProjectSettingsTest,
	"ProjectRift.Settings.ProjectTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
void TestSettingProperty(
	FAutomationTestBase& Test,
	const UClass* SettingsClass,
	const FName PropertyName,
	const TCHAR* ExpectedClampMin,
	const TCHAR* ExpectedClampMax = nullptr)
{
	const FProperty* Property = FindFProperty<FProperty>(SettingsClass, PropertyName);
	Test.TestNotNull(*FString::Printf(TEXT("%s exists"), *PropertyName.ToString()), Property);
	if (!Property)
	{
		return;
	}

	Test.TestTrue(*FString::Printf(TEXT("%s is config-backed"), *PropertyName.ToString()), Property->HasAnyPropertyFlags(CPF_Config));
	Test.TestTrue(*FString::Printf(TEXT("%s is editable"), *PropertyName.ToString()), Property->HasAnyPropertyFlags(CPF_Edit));
	Test.TestTrue(*FString::Printf(TEXT("%s is Blueprint visible"), *PropertyName.ToString()), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
	Test.TestTrue(*FString::Printf(TEXT("%s is Blueprint read-only"), *PropertyName.ToString()), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
#if WITH_METADATA
	Test.TestEqual(*FString::Printf(TEXT("%s ClampMin"), *PropertyName.ToString()), Property->GetMetaData(TEXT("ClampMin")), FString(ExpectedClampMin));
	Test.TestEqual(*FString::Printf(TEXT("%s UIMin"), *PropertyName.ToString()), Property->GetMetaData(TEXT("UIMin")), FString(ExpectedClampMin));

	if (ExpectedClampMax)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s ClampMax"), *PropertyName.ToString()), Property->GetMetaData(TEXT("ClampMax")), FString(ExpectedClampMax));
		Test.TestEqual(*FString::Printf(TEXT("%s UIMax"), *PropertyName.ToString()), Property->GetMetaData(TEXT("UIMax")), FString(ExpectedClampMax));
	}
	else
	{
		Test.TestFalse(*FString::Printf(TEXT("%s has no ClampMax"), *PropertyName.ToString()), Property->HasMetaData(TEXT("ClampMax")));
	}
#endif
}

void TestFloatSettingDefault(
	FAutomationTestBase& Test,
	const UClass* SettingsClass,
	const UObject* Settings,
	const FName PropertyName,
	const float ExpectedValue)
{
	const FFloatProperty* Property = FindFProperty<FFloatProperty>(SettingsClass, PropertyName);
	Test.TestNotNull(*FString::Printf(TEXT("%s is a float setting"), *PropertyName.ToString()), Property);
	if (Property && Settings)
	{
		Test.TestEqual(
			*FString::Printf(TEXT("%s default"), *PropertyName.ToString()),
			Property->GetPropertyValue_InContainer(Settings),
			ExpectedValue);
	}
}

template <typename TValue>
void TestConfigValue(FAutomationTestBase& Test, const TCHAR* Key, const TValue ExpectedValue)
{
	TValue ActualValue{};
	const bool bFound = GConfig->GetValue(TEXT("/Script/ProjectA.PRProjectSettings"), Key, ActualValue, GGameIni);
	Test.TestTrue(*FString::Printf(TEXT("%s is explicitly configured"), Key), bFound);
	Test.TestEqual(*FString::Printf(TEXT("%s config value"), Key), ActualValue, ExpectedValue);
}
}

bool FPRProjectSettingsTest::RunTest(const FString& Parameters)
{
	const UClass* SettingsClass = UPRProjectSettings::StaticClass();
	const UPRProjectSettings* Settings = GetDefault<UPRProjectSettings>();

	TestTrue(TEXT("UPRProjectSettings uses config"), SettingsClass->HasAnyClassFlags(CLASS_Config));
	TestTrue(TEXT("UPRProjectSettings writes defaults to the default config"), SettingsClass->HasAnyClassFlags(CLASS_DefaultConfig));
	TestEqual(TEXT("UPRProjectSettings uses Game config"), SettingsClass->ClassConfigName, FName(TEXT("Game")));
	TestNotNull(TEXT("UPRProjectSettings default object exists"), Settings);
	if (!Settings)
	{
		return false;
	}

	TestEqual(TEXT("Settings category"), Settings->GetCategoryName(), FName(TEXT("ProjectRift")));
	TestEqual(TEXT("Settings section"), Settings->GetSectionName(), FName(TEXT("Gameplay")));

	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, InitialRiftStability), TEXT("0.0"), TEXT("100.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, RiftStabilityDrainPerSecond), TEXT("0.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, FailedResourceRetentionRate), TEXT("0.0"), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ReturnToLobbyDelayAfterSettlement), TEXT("0.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ObjectiveHoldDuration), TEXT("0.1"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ObjectiveInteractionRadius), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, BaseEnemiesPerWave), TEXT("0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, EnemiesPerAdditionalPlayer), TEXT("0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, MaxAliveEnemies), TEXT("1"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, MaxAliveEnemiesPerAdditionalPlayer), TEXT("0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, EnemyHealthMultiplierPerAdditionalPlayer), TEXT("0.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, WaveInterval), TEXT("0.1"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ExtractionRadius), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ReviveHoldDuration), TEXT("0.1"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ReviveInteractionDistance), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ReviveMovementCancelDistance), TEXT("0.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, BleedOutDuration), TEXT("0.1"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, ReviveHealthFraction), TEXT("0.01"), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, GET_MEMBER_NAME_CHECKED(UPRProjectSettings, DroneReviveDuration), TEXT("0.1"));
	TestSettingProperty(*this, SettingsClass, TEXT("AttackPowerDivisor"), TEXT("1.0"));
	TestSettingProperty(*this, SettingsClass, TEXT("MaxPollutionDamageReduction"), TEXT("0.0"), TEXT("1.0"));

	TestEqual(TEXT("Initial stability default"), Settings->InitialRiftStability, 100.0f);
	TestEqual(TEXT("Stability drain default"), Settings->RiftStabilityDrainPerSecond, 0.15f);
	TestEqual(TEXT("Failed retention default"), Settings->FailedResourceRetentionRate, 0.5f);
	TestEqual(TEXT("Return delay default"), Settings->ReturnToLobbyDelayAfterSettlement, 4.0f);
	TestEqual(TEXT("Hold duration default"), Settings->ObjectiveHoldDuration, 30.0f);
	TestEqual(TEXT("Interaction radius default"), Settings->ObjectiveInteractionRadius, 250.0f);
	TestEqual(TEXT("Base enemies default"), Settings->BaseEnemiesPerWave, 2);
	TestEqual(TEXT("Additional enemies default"), Settings->EnemiesPerAdditionalPlayer, 1);
	TestEqual(TEXT("Max alive default"), Settings->MaxAliveEnemies, 8);
	TestEqual(TEXT("Additional max alive enemies default"), Settings->MaxAliveEnemiesPerAdditionalPlayer, 2);
	TestEqual(TEXT("Enemy health multiplier default"), Settings->EnemyHealthMultiplierPerAdditionalPlayer, 0.25f);
	TestEqual(TEXT("Wave interval default"), Settings->WaveInterval, 6.0f);
	TestEqual(TEXT("Extraction radius default"), Settings->ExtractionRadius, 320.0f);
	TestEqual(TEXT("Revive hold duration default"), Settings->ReviveHoldDuration, 3.0f);
	TestEqual(TEXT("Revive interaction distance default"), Settings->ReviveInteractionDistance, 250.0f);
	TestEqual(TEXT("Revive movement cancellation default"), Settings->ReviveMovementCancelDistance, 35.0f);
	TestEqual(TEXT("Bleed-out duration default"), Settings->BleedOutDuration, 30.0f);
	TestEqual(TEXT("Revive health fraction default"), Settings->ReviveHealthFraction, 0.4f);
	TestEqual(TEXT("Rescue drone revive duration default"), Settings->DroneReviveDuration, 3.0f);
	TestFloatSettingDefault(*this, SettingsClass, Settings, TEXT("AttackPowerDivisor"), 100.0f);
	TestFloatSettingDefault(*this, SettingsClass, Settings, TEXT("MaxPollutionDamageReduction"), 0.8f);
	TestFloatSettingDefault(*this, SettingsClass, Settings, TEXT("EncounterMinimumPlayerDistance"), 600.0f);

	TestConfigValue(*this, TEXT("InitialRiftStability"), 100.0f);
	TestConfigValue(*this, TEXT("RiftStabilityDrainPerSecond"), 0.15f);
	TestConfigValue(*this, TEXT("FailedResourceRetentionRate"), 0.5f);
	TestConfigValue(*this, TEXT("ReturnToLobbyDelayAfterSettlement"), 4.0f);
	TestConfigValue(*this, TEXT("ObjectiveHoldDuration"), 30.0f);
	TestConfigValue(*this, TEXT("ObjectiveInteractionRadius"), 250.0f);
	TestConfigValue(*this, TEXT("BaseEnemiesPerWave"), 2);
	TestConfigValue(*this, TEXT("EnemiesPerAdditionalPlayer"), 1);
	TestConfigValue(*this, TEXT("MaxAliveEnemies"), 8);
	TestConfigValue(*this, TEXT("MaxAliveEnemiesPerAdditionalPlayer"), 2);
	TestConfigValue(*this, TEXT("EnemyHealthMultiplierPerAdditionalPlayer"), 0.25f);
	TestConfigValue(*this, TEXT("WaveInterval"), 6.0f);
	TestConfigValue(*this, TEXT("ExtractionRadius"), 320.0f);
	TestConfigValue(*this, TEXT("ReviveHoldDuration"), 3.0f);
	TestConfigValue(*this, TEXT("ReviveInteractionDistance"), 250.0f);
	TestConfigValue(*this, TEXT("ReviveMovementCancelDistance"), 35.0f);
	TestConfigValue(*this, TEXT("BleedOutDuration"), 30.0f);
	TestConfigValue(*this, TEXT("ReviveHealthFraction"), 0.4f);
	TestConfigValue(*this, TEXT("DroneReviveDuration"), 3.0f);
	TestConfigValue(*this, TEXT("AttackPowerDivisor"), 100.0f);
	TestConfigValue(*this, TEXT("MaxPollutionDamageReduction"), 0.8f);
	TestConfigValue(*this, TEXT("EncounterMinimumPlayerDistance"), 600.0f);

	return true;
}

#endif
