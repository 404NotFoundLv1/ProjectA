#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRHealthConsumableGameplayEffect.h"
#include "Abilities/PRShieldConsumableGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "GameplayEffect.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRQuickbarComponent.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "TimerManager.h"
#include "UI/PRInventoryWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPRConsumableUseTest, "ProjectRift.Items.ConsumableUse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FPRItemInstance MakeConsumableUseTestItem(const FName ItemId, const int32 Count)
{
	FPRItemInstance Item;
	Item.ItemId = ItemId;
	Item.Count = Count;
	return Item;
}

bool SetConsumableUseTestItemDataAssets(FAutomationTestBase& Test, UPRInventoryComponent* Inventory, const TArray<UPRItemDataAsset*>& ItemDataAssets)
{
	UFunction* Function = Inventory ? Inventory->FindFunction(TEXT("SetItemDataAssets")) : nullptr;
	Test.TestNotNull(TEXT("Inventory exposes SetItemDataAssets for consumable data"), Function);
	if (!Inventory || !Function)
	{
		return false;
	}

	FStructOnScope Params(Function);
	void* ParamsMemory = Params.GetStructMemory();

	FArrayProperty* AssetsProperty = FindFProperty<FArrayProperty>(Function, TEXT("InItemDataAssets"));
	FObjectProperty* InnerObjectProperty = AssetsProperty ? CastField<FObjectProperty>(AssetsProperty->Inner) : nullptr;
	Test.TestNotNull(TEXT("SetItemDataAssets accepts consumable item data array"), AssetsProperty);
	Test.TestNotNull(TEXT("SetItemDataAssets consumable array stores objects"), InnerObjectProperty);
	if (!AssetsProperty || !InnerObjectProperty)
	{
		return false;
	}

	FScriptArrayHelper AssetsHelper(AssetsProperty, AssetsProperty->ContainerPtrToValuePtr<void>(ParamsMemory));
	for (UPRItemDataAsset* ItemDataAsset : ItemDataAssets)
	{
		const int32 NewIndex = AssetsHelper.AddValue();
		InnerObjectProperty->SetObjectPropertyValue(AssetsHelper.GetRawPtr(NewIndex), ItemDataAsset);
	}

	Inventory->ProcessEvent(Function, ParamsMemory);
	return true;
}

void AdvanceConsumableUseTimer(UWorld* World, const float DurationSeconds)
{
	if (!World)
	{
		return;
	}
	// Test worlds do not advance their timer manager through the editor's normal
	// frame loop. Tick it explicitly so the production timer callback is tested.
	++GFrameCounter;
	World->GetTimerManager().Tick(0.001f);
	++GFrameCounter;
	World->GetTimerManager().Tick(DurationSeconds);
}

struct FConsumableUseTestPlayer
{
	TObjectPtr<APRPlayerController> Controller;
	TObjectPtr<APRPlayerState> PlayerState;
	TObjectPtr<APRCharacter> Character;
	TObjectPtr<UPRInventoryComponent> Inventory;
	TObjectPtr<UPRQuickbarComponent> Quickbar;
	TObjectPtr<UPRAttributeSet> Attributes;
};

FConsumableUseTestPlayer SpawnConsumableUseTestPlayer(FAutomationTestBase& Test, UWorld* World)
{
	FConsumableUseTestPlayer Result;
	if (!World)
	{
		return Result;
	}

	Result.Controller = World->SpawnActor<APRPlayerController>();
	Result.PlayerState = World->SpawnActor<APRPlayerState>();
	Result.Character = World->SpawnActor<APRCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);

	Test.TestNotNull(TEXT("Consumable test controller spawned"), Result.Controller.Get());
	Test.TestNotNull(TEXT("Consumable test player state spawned"), Result.PlayerState.Get());
	Test.TestNotNull(TEXT("Consumable test character spawned"), Result.Character.Get());

	if (!Result.Controller || !Result.PlayerState || !Result.Character)
	{
		return Result;
	}

	Result.Controller->SetPlayerState(Result.PlayerState);
	Result.Controller->Possess(Result.Character);

	Result.Inventory = Result.PlayerState->GetInventoryComponent();
	Result.Quickbar = Result.PlayerState->GetQuickbarComponent();
	Result.Attributes = Result.PlayerState->GetAttributeSet();
	Test.TestNotNull(TEXT("Consumable test inventory exists"), Result.Inventory.Get());
	Test.TestNotNull(TEXT("Consumable test quickbar exists"), Result.Quickbar.Get());
	Test.TestNotNull(TEXT("Consumable test attributes exist"), Result.Attributes.Get());
	Test.TestTrue(TEXT("Consumable test character initializes ASC"), Result.Character->IsAbilitySystemInitialized());

	return Result;
}
}

bool FPRConsumableUseTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Health consumable GE derives from UGameplayEffect"),
		UPRHealthConsumableGameplayEffect::StaticClass()->IsChildOf(UGameplayEffect::StaticClass()));
	TestTrue(
		TEXT("Shield consumable GE derives from UGameplayEffect"),
		UPRShieldConsumableGameplayEffect::StaticClass()->IsChildOf(UGameplayEffect::StaticClass()));

	UClass* PlayerControllerClass = APRPlayerController::StaticClass();
	TestNotNull(TEXT("Player controller exposes UseInventoryItem"), PlayerControllerClass->FindFunctionByName(TEXT("UseInventoryItem")));
	TestNotNull(TEXT("Player controller exposes ServerUseInventoryItem"), PlayerControllerClass->FindFunctionByName(TEXT("ServerUseInventoryItem")));
	UFunction* ServerUseFunction = PlayerControllerClass->FindFunctionByName(TEXT("ServerUseInventoryItem"));
	TestTrue(TEXT("ServerUseInventoryItem is a server RPC"), ServerUseFunction && ServerUseFunction->HasAnyFunctionFlags(FUNC_NetServer));

	UClass* InventoryWidgetClass = UPRInventoryWidget::StaticClass();
	TestNotNull(TEXT("Inventory widget exposes RequestUseSelectedItem"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestUseSelectedItem")));
	TestNotNull(TEXT("Inventory widget exposes RequestUseDisplayedItem"), InventoryWidgetClass->FindFunctionByName(TEXT("RequestUseDisplayedItem")));
	TestNotNull(TEXT("Inventory widget exposes OnUseItemRequested delegate"), FindFProperty<FMulticastDelegateProperty>(InventoryWidgetClass, TEXT("OnUseItemRequested")));

	FTestWorldWrapper WorldWrapper;
	TestTrue(TEXT("Test world is created"), WorldWrapper.CreateTestWorld(EWorldType::Game));
	UWorld* World = WorldWrapper.GetTestWorld();
	if (!World)
	{
		return false;
	}

	TestTrue(TEXT("Test world begins play"), WorldWrapper.BeginPlayInTestWorld());
	if (WorldWrapper.HasFailed())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	FConsumableUseTestPlayer TestPlayer = SpawnConsumableUseTestPlayer(*this, World);
	if (!TestPlayer.Controller || !TestPlayer.Inventory || !TestPlayer.Quickbar || !TestPlayer.Attributes || !TestPlayer.Character)
	{
		return false;
	}

	TestPlayer.Attributes->SetHealth(40.0f);
	TestTrue(TEXT("Can seed HealthInjector stack"), TestPlayer.Inventory->AddItem(MakeConsumableUseTestItem(TEXT("HealthInjector"), 2)));
	TestTrue(TEXT("HealthInjector starts through the controller's timed-use route"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("HealthInjector")));
	TestTrue(TEXT("Direct inventory use retains an active timed transaction"), TestPlayer.Quickbar->GetActiveUse().IsActive());
	TestEqual(TEXT("Direct inventory use has no synthetic quick slot binding"), TestPlayer.Quickbar->GetActiveUse().SlotIndex, INDEX_NONE);
	TestEqual(TEXT("HealthInjector is not consumed before timed completion"), TestPlayer.Inventory->GetItemCount(TEXT("HealthInjector")), 2);
	AdvanceConsumableUseTimer(World, 0.8f);
	TestTrue(TEXT("HealthInjector increases Health"), TestPlayer.Attributes->GetHealth() > 40.0f);
	TestTrue(TEXT("HealthInjector clamps Health to MaxHealth"), TestPlayer.Attributes->GetHealth() <= TestPlayer.Attributes->GetMaxHealth());
	TestEqual(TEXT("HealthInjector consumes one item"), TestPlayer.Inventory->GetItemCount(TEXT("HealthInjector")), 1);

	const float FullHealth = TestPlayer.Attributes->GetMaxHealth();
	TestPlayer.Attributes->SetHealth(FullHealth);
	TestFalse(TEXT("HealthInjector cannot be wasted at full Health"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("HealthInjector")));
	TestEqual(TEXT("Rejected full Health use keeps item count"), TestPlayer.Inventory->GetItemCount(TEXT("HealthInjector")), 1);
	TestEqual(TEXT("Rejected full Health use keeps Health unchanged"), TestPlayer.Attributes->GetHealth(), FullHealth);

	TestPlayer.Attributes->SetShield(10.0f);
	TestTrue(TEXT("Can seed ShieldPack"), TestPlayer.Inventory->AddItem(MakeConsumableUseTestItem(TEXT("ShieldPack"), 1)));
	TestTrue(TEXT("ShieldPack starts through the controller's timed-use route"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("ShieldPack")));
	AdvanceConsumableUseTimer(World, 1.1f);
	TestTrue(TEXT("ShieldPack increases Shield"), TestPlayer.Attributes->GetShield() > 10.0f);
	TestTrue(TEXT("ShieldPack clamps Shield to MaxShield"), TestPlayer.Attributes->GetShield() <= TestPlayer.Attributes->GetMaxShield());
	TestEqual(TEXT("ShieldPack consumes one item"), TestPlayer.Inventory->GetItemCount(TEXT("ShieldPack")), 0);

	const float HealthBeforeMissingItem = TestPlayer.Attributes->GetHealth();
	TestFalse(TEXT("Missing consumable cannot be used"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("ShieldPack")));
	TestEqual(TEXT("Missing consumable does not change Health"), TestPlayer.Attributes->GetHealth(), HealthBeforeMissingItem);

	TestTrue(TEXT("Can seed downed-state HealthInjector"), TestPlayer.Inventory->AddItem(MakeConsumableUseTestItem(TEXT("HealthInjector"), 1)));
	TestPlayer.Attributes->SetHealth(20.0f);
	TestTrue(TEXT("Character can enter downed state for consumable test"), TestPlayer.Character->EnterDownedState());
	TestFalse(TEXT("Downed character cannot use consumables"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("HealthInjector")));
	TestEqual(TEXT("Downed rejected use keeps item count"), TestPlayer.Inventory->GetItemCount(TEXT("HealthInjector")), 2);
	TestEqual(TEXT("Downed rejected use keeps Health unchanged"), TestPlayer.Attributes->GetHealth(), 20.0f);

	TestTrue(TEXT("Character can respawn for data-driven consumable test"), TestPlayer.Character->RespawnFromDowned());
	UPRItemDataAsset* CustomHealthInjectorData = NewObject<UPRItemDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Can create custom consumable item data"), CustomHealthInjectorData);
	if (!CustomHealthInjectorData)
	{
		return false;
	}

	CustomHealthInjectorData->ItemId = TEXT("CustomHealthInjector");
	CustomHealthInjectorData->DisplayName = FText::FromString(TEXT("Custom Health Injector"));
	CustomHealthInjectorData->ItemType = EPRItemType::Consumable;
	CustomHealthInjectorData->MaxStackCount = 5;
	CustomHealthInjectorData->bCanUse = true;
	CustomHealthInjectorData->UseEffect = UPRHealthConsumableGameplayEffect::StaticClass();
	CustomHealthInjectorData->UseDefinition.Kind = EPRItemUseKind::RestoreHealth;
	CustomHealthInjectorData->UseDefinition.bQuickbarEligible = true;
	CustomHealthInjectorData->UseDefinition.UseDurationSeconds = 0.01f;
	TestTrue(TEXT("Can register data-driven consumable item data"), SetConsumableUseTestItemDataAssets(*this, TestPlayer.Inventory, { CustomHealthInjectorData }));

	TestPlayer.Attributes->SetHealth(30.0f);
	TestTrue(TEXT("Can seed custom data-driven consumable"), TestPlayer.Inventory->AddItem(MakeConsumableUseTestItem(TEXT("CustomHealthInjector"), 1)));
	TestTrue(TEXT("Custom consumable starts with its ItemData timed-use definition"), TestPlayer.Controller->TryUseInventoryItemOnServer(TEXT("CustomHealthInjector")));
	AdvanceConsumableUseTimer(World, 0.02f);
	TestTrue(TEXT("Custom consumable increases Health through GameplayEffect"), TestPlayer.Attributes->GetHealth() > 30.0f);
	TestEqual(TEXT("Custom consumable consumes one item"), TestPlayer.Inventory->GetItemCount(TEXT("CustomHealthInjector")), 0);

	return true;
}

#endif
