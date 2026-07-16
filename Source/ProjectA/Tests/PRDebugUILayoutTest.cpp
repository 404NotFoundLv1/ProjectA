#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "UI/PRGASDebugWidget.h"
#include "UI/PRDebugUILayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPRDebugUILayoutTest,
	"ProjectRift.UI.DebugLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPRDebugUILayoutTest::RunTest(const FString& Parameters)
{
	const FVector2D MinimumSupportedViewport(1024.0f, 480.0f);
	auto CalculatePanelMin = [&MinimumSupportedViewport](
		const FAnchors Anchors,
		const FVector2D Alignment,
		const FVector2D Position,
		const FVector2D Size)
	{
		return (Anchors.Minimum * MinimumSupportedViewport) + Position - (Alignment * Size);
	};
	auto RectanglesOverlap = [](const FVector2D FirstMin, const FVector2D FirstSize, const FVector2D SecondMin, const FVector2D SecondSize)
	{
		const FVector2D FirstMax = FirstMin + FirstSize;
		const FVector2D SecondMax = SecondMin + SecondSize;
		return FirstMin.X < SecondMax.X && FirstMax.X > SecondMin.X &&
			FirstMin.Y < SecondMax.Y && FirstMax.Y > SecondMin.Y;
	};

	TestEqual(
		TEXT("Profile anchor is right-top"),
		FPRDebugUILayout::ProfileAnchors().Minimum,
		FVector2D(1.0f, 0.0f));
	TestEqual(
		TEXT("Profile uses a point anchor"),
		FPRDebugUILayout::ProfileAnchors().Maximum,
		FPRDebugUILayout::ProfileAnchors().Minimum);
	TestEqual(
		TEXT("Profile aligns from right-top"),
		FPRDebugUILayout::ProfileAlignment(),
		FVector2D(1.0f, 0.0f));
	TestTrue(
		TEXT("Profile remains inside right edge"),
		FPRDebugUILayout::ProfilePosition().X < 0.0f);
	TestEqual(
		TEXT("GAS anchor is left-top"),
		FPRDebugUILayout::GASAnchors().Minimum,
		FVector2D::ZeroVector);
	TestEqual(
		TEXT("GAS uses a point anchor"),
		FPRDebugUILayout::GASAnchors().Maximum,
		FPRDebugUILayout::GASAnchors().Minimum);
	TestEqual(
		TEXT("Ready anchor is left-bottom"),
		FPRDebugUILayout::LobbyReadyAnchors().Minimum,
		FVector2D(0.0f, 1.0f));
	TestEqual(
		TEXT("Ready aligns from left-bottom"),
		FPRDebugUILayout::LobbyReadyAlignment(),
		FVector2D(0.0f, 1.0f));
	TestEqual(
		TEXT("Ready uses a point anchor"),
		FPRDebugUILayout::LobbyReadyAnchors().Maximum,
		FPRDebugUILayout::LobbyReadyAnchors().Minimum);
	TestTrue(
		TEXT("Top panels use distinct horizontal regions"),
		FPRDebugUILayout::ProfileAnchors().Minimum.X > FPRDebugUILayout::GASAnchors().Minimum.X);
	TestTrue(
		TEXT("Ready is below GAS"),
		FPRDebugUILayout::LobbyReadyAnchors().Minimum.Y > FPRDebugUILayout::GASAnchors().Minimum.Y);
	TestTrue(TEXT("GAS panel is tall enough for combat attributes and statuses"), FPRDebugUILayout::GASSize().Y >= 232.0f);

	UClass* GASDebugWidgetClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PRGASDebugWidget"));
	UClass* EnemyClass = FindObject<UClass>(nullptr, TEXT("/Script/ProjectA.PREnemyCharacter"));
	TestNotNull(TEXT("GAS debug widget exposes debug text for verification"), GASDebugWidgetClass ? GASDebugWidgetClass->FindFunctionByName(TEXT("GetDebugText")) : nullptr);
	UPRAbilitySystemComponent* UninitializedAbilitySystem = NewObject<UPRAbilitySystemComponent>();
	const FString CooldownText = UPRGASDebugWidget::GetCooldownDebugText(UninitializedAbilitySystem);
	TestTrue(TEXT("GAS debug cooldown output includes Q unavailable state"), CooldownText.Contains(TEXT("Q: Unavailable")));
	TestTrue(TEXT("GAS debug cooldown output includes E unavailable state"), CooldownText.Contains(TEXT("E: Unavailable")));
	TestTrue(TEXT("GAS debug cooldown output includes R unavailable state"), CooldownText.Contains(TEXT("R: Unavailable")));
	TestNotNull(TEXT("Enemy exposes active status text for its health bar"), EnemyClass ? EnemyClass->FindFunctionByName(TEXT("GetActiveStatusText")) : nullptr);

	const FVector2D ProfileMin = CalculatePanelMin(
		FPRDebugUILayout::ProfileAnchors(),
		FPRDebugUILayout::ProfileAlignment(),
		FPRDebugUILayout::ProfilePosition(),
		FPRDebugUILayout::ProfileSize());
	const FVector2D GASMin = CalculatePanelMin(
		FPRDebugUILayout::GASAnchors(),
		FPRDebugUILayout::GASAlignment(),
		FPRDebugUILayout::GASPosition(),
		FPRDebugUILayout::GASSize());
	const FVector2D ReadyMin = CalculatePanelMin(
		FPRDebugUILayout::LobbyReadyAnchors(),
		FPRDebugUILayout::LobbyReadyAlignment(),
		FPRDebugUILayout::LobbyReadyPosition(),
		FPRDebugUILayout::LobbyReadySize());
	TestFalse(
		TEXT("Profile and GAS panels do not overlap at the supported minimum viewport"),
		RectanglesOverlap(ProfileMin, FPRDebugUILayout::ProfileSize(), GASMin, FPRDebugUILayout::GASSize()));
	TestFalse(
		TEXT("Profile and Ready panels do not overlap at the supported minimum viewport"),
		RectanglesOverlap(ProfileMin, FPRDebugUILayout::ProfileSize(), ReadyMin, FPRDebugUILayout::LobbyReadySize()));
	TestFalse(
		TEXT("GAS and Ready panels do not overlap at the supported minimum viewport"),
		RectanglesOverlap(GASMin, FPRDebugUILayout::GASSize(), ReadyMin, FPRDebugUILayout::LobbyReadySize()));

	return true;
}

#endif
