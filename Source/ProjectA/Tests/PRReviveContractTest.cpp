#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/PRCharacter.h"
#include "Core/PRRiftGameState.h"
#include "Core/PRSpawnManager.h"
#include "Enemies/PREnemyCharacter.h"
#include "GameplayTagsManager.h"
#include "Settings/PRProjectSettings.h"
#include "UI/PRWeaponHUDWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRReviveContractTest,
	"ProjectRift.Revive.Contracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRReviveContractTest::RunTest(const FString& Parameters)
{
	UClass* const ReviveComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRReviveComponent"));
	UClass* const RescueDroneClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRRescueDroneActor"));
	TestNotNull(TEXT("Native revive component class exists"), ReviveComponentClass);
	TestNotNull(TEXT("Native rescue drone actor class exists"), RescueDroneClass);

	TestTrue(
		TEXT("State.Reviving native gameplay tag exists"),
		FGameplayTag::RequestGameplayTag(TEXT("State.Reviving"), false).IsValid());
	TestTrue(
		TEXT("State.BeingRevived native gameplay tag exists"),
		FGameplayTag::RequestGameplayTag(TEXT("State.BeingRevived"), false).IsValid());

	UClass* const CharacterClass = APRCharacter::StaticClass();
	TestNotNull(TEXT("APRCharacter exposes revive component"), CharacterClass->FindFunctionByName(TEXT("GetReviveComponent")));
	TestNotNull(TEXT("APRCharacter exposes interaction-release handler"), CharacterClass->FindFunctionByName(TEXT("DoInteractReleased")));
	TestNotNull(TEXT("Weapon HUD exposes contextual revive status"), UPRWeaponHUDWidget::StaticClass()->FindFunctionByName(TEXT("GetReviveStatusText")));

	UClass* const RiftGameStateClass = APRRiftGameState::StaticClass();
	TestNotNull(TEXT("Rift GameState exposes frozen difficulty player count"), RiftGameStateClass->FindFunctionByName(TEXT("GetDifficultyPlayerCount")));

	UClass* const SpawnManagerClass = APRSpawnManager::StaticClass();
	TestNotNull(TEXT("Spawn manager exposes frozen scaling player count"), SpawnManagerClass->FindFunctionByName(TEXT("GetDifficultyPlayerCountForScaling")));
	TestNotNull(TEXT("Spawn manager exposes scaled alive-enemy cap"), SpawnManagerClass->FindFunctionByName(TEXT("GetMaxAliveEnemiesForScaling")));
	TestNotNull(TEXT("Spawn manager exposes enemy health multiplier"), SpawnManagerClass->FindFunctionByName(TEXT("GetEnemyHealthMultiplierForScaling")));

	UClass* const EnemyClass = APREnemyCharacter::StaticClass();
	TestNotNull(TEXT("Enemy exposes pre-spawn health multiplier"), EnemyClass->FindFunctionByName(TEXT("SetSpawnHealthMultiplier")));

	UClass* const SettingsClass = UPRProjectSettings::StaticClass();
	for (const FName RequiredProperty : {
		FName(TEXT("ReviveHoldDuration")),
		FName(TEXT("ReviveInteractionDistance")),
		FName(TEXT("ReviveMovementCancelDistance")),
		FName(TEXT("BleedOutDuration")),
		FName(TEXT("ReviveHealthFraction")),
		FName(TEXT("DroneReviveDuration")),
		FName(TEXT("MaxAliveEnemiesPerAdditionalPlayer")),
		FName(TEXT("EnemyHealthMultiplierPerAdditionalPlayer")) })
	{
		TestNotNull(
			FString::Printf(TEXT("Project settings expose %s"), *RequiredProperty.ToString()),
			SettingsClass->FindPropertyByName(RequiredProperty));
	}

	return true;
}

#endif
