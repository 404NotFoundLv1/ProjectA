#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRAssetManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GeneralProjectSettings.h"
#include "HAL/FileManager.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Misc/Paths.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Ship/PRShipRepairDataAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRSmokeContentTest,
	"ProjectRift.Smoke.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSmokeContentTest::RunTest(const FString& Parameters)
{
	const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>();
	TestNotNull(TEXT("General project settings exist"), ProjectSettings);
	TestEqual(
		TEXT("Smoke suite is bound to ProjectRift v0.8.1"),
		ProjectSettings ? ProjectSettings->ProjectVersion : FString(),
		FString(TEXT("0.8.1")));

	const TCHAR* RequiredMaps[] = {
		TEXT("/Game/ProjectRift/Maps/L_MainMenu.L_MainMenu"),
		TEXT("/Game/ProjectRift/Maps/L_ShipLobby.L_ShipLobby"),
		TEXT("/Game/ProjectRift/Maps/L_Rift_Test.L_Rift_Test")
	};
	for (const TCHAR* MapPath : RequiredMaps)
	{
		TestNotNull(
			FString::Printf(TEXT("Required smoke map loads: %s"), MapPath),
			LoadObject<UWorld>(nullptr, MapPath));
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TestNotNull(TEXT("ProjectRift AssetManager is active"), AssetManager);
	if (!AssetManager)
	{
		return false;
	}

	TestNotNull(TEXT("EnergyCrystal item loads"), AssetManager->LoadItemDataSync(TEXT("EnergyCrystal")));
	TestNotNull(TEXT("Test loot table loads"), AssetManager->LoadLootTableSync(TEXT("DA_TestLootTable")));
	TestNotNull(TEXT("Starter mission loads"), AssetManager->LoadMissionSync(TEXT("Mission.Rift.Test.Hold")));
	TestNotNull(TEXT("Engine repair contract loads"), AssetManager->LoadShipRepairSync(TEXT("Repair.Ship.Engine.Stage1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRSmokeProfileIsolationTest,
	"ProjectRift.Smoke.ProfileIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSmokeProfileIsolationTest::RunTest(const FString& Parameters)
{
	const FString RunIdText = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const FString ProjectAutomationRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
	const FString RunRoot = FPaths::Combine(ProjectAutomationRoot, TEXT("Gauntlet"), RunIdText);
	const FString CustomUserDir = FPaths::Combine(RunRoot, TEXT("UserDir"));
	FString ProfileRoot;
	FString Diagnostic;
	TestTrue(
		TEXT("Valid Gauntlet run resolves an isolated profile root"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			CustomUserDir,
			ProjectAutomationRoot,
			ProfileRoot,
			Diagnostic));
	TestEqual(
		TEXT("Profile root is derived from the explicit ProjectA Automation root rather than engine -userdir"),
		FPaths::ConvertRelativePathToFull(ProfileRoot),
		FPaths::ConvertRelativePathToFull(FPaths::Combine(RunRoot, TEXT("Profiles"))));
	TestTrue(
		TEXT("Resolved profile root stays under ProjectA Saved/Automation"),
		FPaths::IsUnderDirectory(
			FPaths::ConvertRelativePathToFull(ProfileRoot),
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation")))));
	TestTrue(TEXT("Resolved profile root names the run"), ProfileRoot.Contains(RunIdText));
	TestTrue(TEXT("Resolved profile root ends in Profiles"), ProfileRoot.EndsWith(TEXT("Profiles")));

	FString RejectedRoot;
	FString RejectedDiagnostic;
	TestFalse(
		TEXT("Missing custom user directory is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			FString(),
			ProjectAutomationRoot,
			RejectedRoot,
			RejectedDiagnostic));
	TestFalse(
		TEXT("Invalid run GUID is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			TEXT("not-a-guid"),
			CustomUserDir,
			ProjectAutomationRoot,
			RejectedRoot,
			RejectedDiagnostic));
	TestFalse(
		TEXT("Relative custom user directory is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			TEXT("Relative/UserDir"),
			ProjectAutomationRoot,
			RejectedRoot,
			RejectedDiagnostic));
	TestFalse(
		TEXT("Custom user directory inside real ProjectA save data is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("ProjectRift")),
			ProjectAutomationRoot,
			RejectedRoot,
			RejectedDiagnostic));
	TestFalse(
		TEXT("Relative ProjectA Automation root is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			CustomUserDir,
			TEXT("Saved/Automation"),
			RejectedRoot,
			RejectedDiagnostic));
	TestFalse(
		TEXT("A root not ending in Saved/Automation is rejected"),
		UPRSaveSubsystem::ResolveGauntletSmokeProfileRoot(
			RunIdText,
			CustomUserDir,
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Other")),
			RejectedRoot,
			RejectedDiagnostic));

	IFileManager::Get().DeleteDirectory(*ProfileRoot, false, true);
	UGameInstance* FirstGameInstance = NewObject<UGameInstance>();
	UPRSaveSubsystem* FirstSubsystem = NewObject<UPRSaveSubsystem>(FirstGameInstance);
	FirstSubsystem->InitializeForAutomation(ProfileRoot);
	const FPRProfileOperationResult CreateResult = FirstSubsystem->CreateProfile(TEXT("Gauntlet Smoke Profile"));
	TestTrue(TEXT("Isolated smoke profile is created"), CreateResult.IsSuccess());

	FPRMultiplayerProfileProjection Projection;
	const FPRProfileOperationResult ProjectionResult = FirstSubsystem->BuildMultiplayerProfileProjection(Projection);
	TestTrue(TEXT("Isolated profile builds multiplayer projection"), ProjectionResult.IsSuccess());
	TestTrue(TEXT("Projection owns a valid profile GUID"), Projection.ProfileId.IsValid());

	UGameInstance* ReloadGameInstance = NewObject<UGameInstance>();
	UPRSaveSubsystem* ReloadSubsystem = NewObject<UPRSaveSubsystem>(ReloadGameInstance);
	ReloadSubsystem->InitializeForAutomation(ProfileRoot);
	TestEqual(TEXT("Reload keeps the isolated active profile"), ReloadSubsystem->GetActiveProfileId(), Projection.ProfileId);
	TestTrue(TEXT("Reloaded subsystem remains isolated"), ReloadSubsystem->IsUsingIsolatedDevelopmentRoot());

	IFileManager::Get().DeleteDirectory(*ProfileRoot, false, true);
	return true;
}

#endif
