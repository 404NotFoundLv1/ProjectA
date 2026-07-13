#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GauntletTestController.h"
#include "PRLocalSmokeGauntletController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRSmokeGauntletContractTest,
	"ProjectRift.Smoke.GauntletContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRSmokeGauntletContractTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("ProjectRift smoke controller derives from UGauntletTestController"),
		UPRLocalSmokeGauntletController::StaticClass()->IsChildOf(UGauntletTestController::StaticClass()));
	TestEqual(TEXT("Each smoke stage times out after thirty seconds"), UPRLocalSmokeGauntletController::StageTimeoutSeconds, 30.0f);
	TestEqual(TEXT("Full smoke run times out after one hundred eighty seconds"), UPRLocalSmokeGauntletController::TotalTimeoutSeconds, 180.0f);

	TestTrue(
		TEXT("Lobby wait can advance to profile creation"),
		UPRLocalSmokeGauntletController::IsAllowedTransition(
			EPRLocalSmokeStage::WaitingForLobby,
			EPRLocalSmokeStage::CreatingProfile));
	TestFalse(
		TEXT("Duplicate stage callbacks cannot transition again"),
		UPRLocalSmokeGauntletController::IsAllowedTransition(
			EPRLocalSmokeStage::CreatingProfile,
			EPRLocalSmokeStage::CreatingProfile));
	TestFalse(
		TEXT("Smoke flow cannot skip from lobby wait directly to passed"),
		UPRLocalSmokeGauntletController::IsAllowedTransition(
			EPRLocalSmokeStage::WaitingForLobby,
			EPRLocalSmokeStage::Passed));
	TestTrue(
		TEXT("Any active stage can fail"),
		UPRLocalSmokeGauntletController::IsAllowedTransition(
			EPRLocalSmokeStage::WaitingForSettlement,
			EPRLocalSmokeStage::Failed));
	return true;
}

#endif
