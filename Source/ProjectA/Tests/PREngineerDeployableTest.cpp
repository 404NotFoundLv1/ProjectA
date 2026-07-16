#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_EngineerRepairDrone.h"
#include "Abilities/GA_EngineerSentry.h"
#include "Abilities/GA_EngineerShieldGenerator.h"
#include "Deployables/PRDeployableActor.h"
#include "Deployables/PRDeployableComponent.h"
#include "Roles/PREngineerModuleDataAsset.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPREngineerDeployableDefinitionTest,
	"ProjectRift.Engineer.DeployableDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPREngineerDeployableDefinitionTest::RunTest(const FString& Parameters)
{
	UPREngineerModuleDataAsset* Module = NewObject<UPREngineerModuleDataAsset>(GetTransientPackage());
	Module->ModuleId = TEXT("Ability.Module.Engineer.Sentry");
	Module->RoleId = TEXT("Ability.Role.Engineer");
	Module->Slot = EPRRoleModuleSlot::Q;
	Module->GameplayAbilityClass = UGA_AssaultCharge::StaticClass();
	Module->DisplayName = FText::FromString(TEXT("Sentry"));
	Module->DeployableKind = EPRDeployableKind::Sentry;
	Module->DeployableActorClass = APRDeployableActor::StaticClass();
	Module->PlacementRange = 1600.0f;
	Module->MaxSurfaceSlopeDegrees = 35.0f;
	Module->LifetimeSeconds = 12.0f;
	Module->EffectRange = 1600.0f;
	Module->EffectIntervalSeconds = 0.6f;
	Module->PrimaryMagnitude = 5.0f;

	FString Diagnostic;
	TestTrue(TEXT("A finite sentry deployment definition is valid"), Module->ValidateDefinition(&Diagnostic));

	Module->PlacementRange = -1.0f;
	TestFalse(TEXT("Deployment definitions reject a negative placement range"), Module->ValidateDefinition(&Diagnostic));
	Module->PlacementRange = 1600.0f;

	TestNotNull(TEXT("PlayerState deployment coordinator class exists"), UPRDeployableComponent::StaticClass());
	TestNotNull(TEXT("Engineer sentry ability class exists"), UGA_EngineerSentry::StaticClass());
	TestNotNull(TEXT("Engineer repair drone ability class exists"), UGA_EngineerRepairDrone::StaticClass());
	TestNotNull(TEXT("Engineer shield generator ability class exists"), UGA_EngineerShieldGenerator::StaticClass());
	TestTrue(TEXT("Finite placement transforms are accepted"), UPRDeployableComponent::IsFinitePlacementTransform(FTransform::Identity));
	TestFalse(
		TEXT("Non-finite placement transforms are rejected"),
		UPRDeployableComponent::IsFinitePlacementTransform(FTransform(FQuat::Identity, FVector(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f))));
	return true;
}

#endif
