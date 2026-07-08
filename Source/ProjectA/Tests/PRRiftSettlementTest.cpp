#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/PRRiftSettlementTypes.h"
#include "Player/PRPlayerController.h"
#include "UI/PRRiftSettlementWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRRiftSettlementTest, "ProjectRift.Rift.Settlement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool SetSettlementIntField(FPRRiftSettlementData& SettlementData, const FName FieldName, const int32 Value)
{
	FIntProperty* Property = FindFProperty<FIntProperty>(FPRRiftSettlementData::StaticStruct(), FieldName);
	if (!Property)
	{
		return false;
	}

	Property->SetPropertyValue_InContainer(&SettlementData, Value);
	return true;
}

int32 GetSettlementIntField(const FPRRiftSettlementData& SettlementData, const FName FieldName)
{
	const FIntProperty* Property = FindFProperty<FIntProperty>(FPRRiftSettlementData::StaticStruct(), FieldName);
	return Property ? Property->GetPropertyValue_InContainer(&SettlementData) : 0;
}
}

bool FPRRiftSettlementTest::RunTest(const FString& Parameters)
{
	UClass* SettlementWidgetClass = UPRRiftSettlementWidget::StaticClass();
	UClass* PlayerControllerClass = APRPlayerController::StaticClass();

	TestNotNull(TEXT("Rift settlement widget class exists"), SettlementWidgetClass);
	TestTrue(
		TEXT("Rift settlement widget derives from UUserWidget"),
		SettlementWidgetClass && SettlementWidgetClass->IsChildOf(UUserWidget::StaticClass()));
	TestNotNull(TEXT("Settlement widget exposes SetPreviewSettlementData"), SettlementWidgetClass->FindFunctionByName(TEXT("SetPreviewSettlementData")));
	TestNotNull(TEXT("Settlement widget exposes GetDisplayedSettlementData"), SettlementWidgetClass->FindFunctionByName(TEXT("GetDisplayedSettlementData")));
	TestNotNull(TEXT("Settlement widget exposes GetDisplayedSettlementText"), SettlementWidgetClass->FindFunctionByName(TEXT("GetDisplayedSettlementText")));
	TestNotNull(TEXT("PlayerController exposes GetRiftSettlementWidget"), PlayerControllerClass->FindFunctionByName(TEXT("GetRiftSettlementWidget")));
	TestNotNull(TEXT("PlayerController has configurable RiftSettlementWidgetClass"), FindFProperty<FClassProperty>(PlayerControllerClass, TEXT("RiftSettlementWidgetClass")));
	TestNotNull(TEXT("Rift settlement data records killed enemy count"), FindFProperty<FIntProperty>(FPRRiftSettlementData::StaticStruct(), TEXT("KilledEnemyCount")));

	UPRRiftSettlementWidget* Widget = NewObject<UPRRiftSettlementWidget>(GetTransientPackage());
	TestNotNull(TEXT("Can instantiate native rift settlement widget"), Widget);
	if (!Widget)
	{
		return false;
	}

	FPRRiftSettlementData SettlementData;
	SettlementData.Result = EPRRiftMissionResult::Success;
	SettlementData.MissionTime = 42.5f;
	SettlementData.RiftStability = 68.0f;
	SettlementData.ObjectiveProgress = 0.75f;
	SettlementData.AlivePlayerCount = 2;
	SettlementData.ExtractedPlayerCount = 2;
	SettlementData.ExtractedItemCount = 5;
	SettlementData.ExtractedUniqueItemTypes = 3;
	TestTrue(TEXT("Can seed settlement killed enemy count"), SetSettlementIntField(SettlementData, TEXT("KilledEnemyCount"), 4));

	Widget->SetPreviewSettlementData(SettlementData);

	const FPRRiftSettlementData DisplayedData = Widget->GetDisplayedSettlementData();
	TestEqual(TEXT("Settlement widget stores server settlement result"), DisplayedData.Result, EPRRiftMissionResult::Success);
	TestEqual(TEXT("Settlement widget stores extracted item count"), DisplayedData.ExtractedItemCount, 5);
	TestEqual(TEXT("Settlement widget stores killed enemy count"), GetSettlementIntField(DisplayedData, TEXT("KilledEnemyCount")), 4);

	const FString SettlementText = Widget->GetDisplayedSettlementText().ToString();
	TestTrue(TEXT("Settlement widget renders Chinese settlement title"), SettlementText.Contains(TEXT("\u526F\u672C\u7ED3\u7B97")));
	TestTrue(TEXT("Settlement widget renders success result"), SettlementText.Contains(TEXT("\u64A4\u79BB\u6210\u529F")));
	TestTrue(TEXT("Settlement widget renders mission time"), SettlementText.Contains(TEXT("42.5")));
	TestTrue(TEXT("Settlement widget renders killed enemy label"), SettlementText.Contains(TEXT("\u51FB\u6740\u6570")));
	TestTrue(TEXT("Settlement widget renders killed enemy count"), SettlementText.Contains(TEXT("4")));
	TestTrue(TEXT("Settlement widget renders carried item count"), SettlementText.Contains(TEXT("5")));

	Widget->ClearPreviewSettlementData();
	TestFalse(TEXT("Clearing preview removes displayed settlement data"), Widget->GetDisplayedSettlementData().IsValid());
	TestNotEqual(
		TEXT("Settlement widget remains tickable while waiting for replicated GameState data"),
		Widget->GetVisibility(),
		ESlateVisibility::Collapsed);

	return true;
}

#endif
