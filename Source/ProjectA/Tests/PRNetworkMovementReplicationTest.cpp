#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/PRCharacter.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRNetworkMovementReplicationTest, "ProjectRift.Network.MovementReplicationSetup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRNetworkMovementReplicationTest::RunTest(const FString& Parameters)
{
	const APRCharacter* CharacterDefaults = GetDefault<APRCharacter>();

	TestTrue(TEXT("ProjectRift character actor replication is enabled"), CharacterDefaults->GetIsReplicated());
	TestTrue(TEXT("ProjectRift character movement replication is enabled on the actor"), CharacterDefaults->IsReplicatingMovement());
	TestNotNull(TEXT("ProjectRift character keeps the engine CharacterMovement component"), CharacterDefaults->GetCharacterMovement());
	TestNotNull(TEXT("ProjectRift character exposes a multiplayer debug label"), CharacterDefaults->GetPlayerDebugLabel());

	return true;
}

#endif
