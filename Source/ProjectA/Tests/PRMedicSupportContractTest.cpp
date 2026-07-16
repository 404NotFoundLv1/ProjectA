#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRCombatEffectLibrary.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagsManager.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRMedicSupportContractTest,
	"ProjectRift.Medic.SupportContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRMedicSupportContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Data.Healing is a native gameplay tag"),
		UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Data.Healing")), false).IsValid());
	TestTrue(
		TEXT("Status.Debuff.Revealed is a native gameplay tag"),
		UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Status.Debuff.Revealed")), false).IsValid());
	for (const TCHAR* CueTagName :
		{
			TEXT("GameplayCue.Combat.Support.Healed"),
			TEXT("GameplayCue.Combat.Support.Purified"),
			TEXT("GameplayCue.Combat.Status.Revealed"),
		})
	{
		TestTrue(
			*FString::Printf(TEXT("%s is a native gameplay tag"), CueTagName),
			UGameplayTagsManager::Get().RequestGameplayTag(FName(CueTagName), false).IsValid());
	}
	TestNotNull(
		TEXT("Combat library exposes health healing"),
		UPRCombatEffectLibrary::StaticClass()->FindFunctionByName(TEXT("ApplyHealthHealingToTarget")));
	TestNotNull(
		TEXT("Combat library exposes selective purification"),
		UPRCombatEffectLibrary::StaticClass()->FindFunctionByName(TEXT("ClearPurifiableStatusEffects")));
	TestNotNull(
		TEXT("Health healing GameplayEffect exists"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRHealthHealingGameplayEffect")));
	TestNotNull(
		TEXT("Recon reveal GameplayEffect exists"),
		FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRReconRevealGameplayEffect")));
	UClass* WeaponHUDClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRWeaponHUDWidget"));
	TestNotNull(TEXT("Weapon HUD exists for recon markers"), WeaponHUDClass);
	if (WeaponHUDClass)
	{
		TestNotNull(
			TEXT("Weapon HUD exposes revealed-enemy count"),
			WeaponHUDClass->FindFunctionByName(TEXT("GetRevealedEnemyCount")));
	}

	struct FExpectedCueAsset
	{
		const TCHAR* ClassPath;
		const TCHAR* CueTagName;
	};
	for (const FExpectedCueAsset& ExpectedCue :
		{
			FExpectedCueAsset{TEXT("/Game/ProjectRift/GameplayCues/Support/GC_Combat_Support_Healed.GC_Combat_Support_Healed_C"), TEXT("GameplayCue.Combat.Support.Healed")},
			FExpectedCueAsset{TEXT("/Game/ProjectRift/GameplayCues/Support/GC_Combat_Support_Purified.GC_Combat_Support_Purified_C"), TEXT("GameplayCue.Combat.Support.Purified")},
			FExpectedCueAsset{TEXT("/Game/ProjectRift/GameplayCues/Status/GC_Combat_Status_Revealed.GC_Combat_Status_Revealed_C"), TEXT("GameplayCue.Combat.Status.Revealed")},
		})
	{
		UClass* CueClass = LoadClass<UGameplayCueNotify_Static>(nullptr, ExpectedCue.ClassPath);
		TestNotNull(*FString::Printf(TEXT("%s cue Blueprint class exists"), ExpectedCue.CueTagName), CueClass);
		const UGameplayCueNotify_Static* CueCDO = CueClass ? Cast<UGameplayCueNotify_Static>(CueClass->GetDefaultObject()) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s cue Blueprint has a CDO"), ExpectedCue.CueTagName), CueCDO);
		if (CueCDO)
		{
			TestEqual(
				*FString::Printf(TEXT("%s cue Blueprint retains its configured tag"), ExpectedCue.CueTagName),
				CueCDO->GameplayCueTag.ToString(),
				FString(ExpectedCue.CueTagName));
		}
	}
	return true;
}

#endif
