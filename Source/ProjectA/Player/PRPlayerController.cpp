#include "Player/PRPlayerController.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRHealthConsumableGameplayEffect.h"
#include "Abilities/PRShieldConsumableGameplayEffect.h"
#include "Characters/PRCharacter.h"
#include "Core/PRAssetManager.h"
#include "Core/PRGameState.h"
#include "Core/PRShipLobbyGameMode.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRLootTableLibrary.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "Persistence/PRSaveSubsystem.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "UI/PRDebugUILayout.h"
#include "UI/PRGASDebugWidget.h"
#include "UI/PRInventoryWidget.h"
#include "UI/PRLobbyReadyDebugWidget.h"
#include "UI/PRRiftSettlementWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName AssaultRoleModuleName(TEXT("Ability.Role.Assault"));
const FName HealthInjectorItemId(TEXT("HealthInjector"));
const FName ShieldPackItemId(TEXT("ShieldPack"));

bool IsShipLobbyDebugWorld(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	if (const AGameModeBase* AuthGameMode = World->GetAuthGameMode())
	{
		return AuthGameMode->IsA<APRShipLobbyGameMode>();
	}

	return World->GetMapName().Contains(TEXT("L_ShipLobby"));
}

bool FindInventoryItemById(const UPRInventoryComponent* InventoryComponent, const FName ItemId, FPRItemInstance& OutItem)
{
	if (!InventoryComponent || ItemId.IsNone())
	{
		return false;
	}

	for (const FPRItemInstance& Item : InventoryComponent->GetItems())
	{
		if (Item.ItemId == ItemId)
		{
			OutItem = Item;
			return true;
		}
	}

	return false;
}

FString GetLobbyReadyLine(const APlayerState* PlayerState)
{
	const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
	const bool bReady = ProjectRiftPlayerState && ProjectRiftPlayerState->IsReady();
	const FName SelectedRoleModule = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetSelectedRoleModule() : NAME_None;
	const FString ResourceSummary = ProjectRiftPlayerState ? ProjectRiftPlayerState->BuildShipResourceSummary() : FString(TEXT("None"));
	FString DisplayName = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetPlayerDisplayName() : FString();

	if (DisplayName.IsEmpty() && PlayerState)
	{
		DisplayName = PlayerState->GetPlayerName();
	}

	if (DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Player");
	}

	const int32 PlayerId = PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
	return FString::Printf(
		TEXT("P%d %s: %s | Profile: %s | Role: %s | Resources: %s"),
		PlayerId,
		*DisplayName,
		bReady ? TEXT("READY") : TEXT("WAITING"),
		ProjectRiftPlayerState && ProjectRiftPlayerState->IsMultiplayerProfileBound() ? TEXT("BOUND") : TEXT("UNBOUND"),
		SelectedRoleModule.IsNone() ? TEXT("None") : *SelectedRoleModule.ToString(),
		*ResourceSummary);
}
}

APRPlayerController::APRPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextAsset(TEXT("/Game/ProjectRift/Input/IMC_Default.IMC_Default"));
	if (DefaultContextAsset.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContextAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContextAsset(TEXT("/Game/ProjectRift/Input/IMC_MouseLook.IMC_MouseLook"));
	if (MouseLookContextAsset.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookContextAsset.Object);
	}

	static ConstructorHelpers::FClassFinder<UPRGASDebugWidget> GASDebugWidgetAsset(TEXT("/Game/ProjectRift/UI/WBP_GASDebugHUD"));
	if (GASDebugWidgetAsset.Succeeded())
	{
		GASDebugWidgetClass = GASDebugWidgetAsset.Class;
	}

	InventoryWidgetClass = UPRInventoryWidget::StaticClass();
	RiftSettlementWidgetClass = UPRRiftSettlementWidget::StaticClass();
	HealthInjectorEffectClass = UPRHealthConsumableGameplayEffect::StaticClass();
	ShieldPackEffectClass = UPRShieldConsumableGameplayEffect::StaticClass();
	PickupActorClass = APRPickupActor::StaticClass();
}

void APRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		GetWorldTimerManager().SetTimer(
			LobbyReadyDebugTimerHandle,
			this,
			&APRPlayerController::RefreshLobbyReadyDebugDisplay,
			1.0f,
			true,
			0.25f);

		CreateGASDebugHUD();
		CreateInventoryUI();
		CreateRiftSettlementUI();
		SubmitLocalMultiplayerProfile();
	}
}

void APRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyRiftSettlementUI();
	DestroyInventoryUI();
	DestroyLobbyReadyDebugHUD();
	DestroyGASDebugHUD();
	GetWorldTimerManager().ClearTimer(LobbyReadyDebugTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void APRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &APRPlayerController::SelectAssaultRoleModule);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APRPlayerController::ToggleReady);
		InputComponent->BindKey(EKeys::O, IE_Pressed, this, &APRPlayerController::StartRiftMission);
	}
}

void APRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	RefreshInventoryDisplay();
	SubmitLocalMultiplayerProfile();
}

void APRPlayerController::SelectAssaultRoleModule()
{
	SelectRoleModule(AssaultRoleModuleName);
}

void APRPlayerController::SelectRoleModule(const FName RoleModule)
{
	ServerSetSelectedRoleModule(RoleModule);
}

void APRPlayerController::ToggleReady()
{
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const bool bNewReadyState = ProjectRiftPlayerState ? !ProjectRiftPlayerState->IsReady() : true;

	ServerSetReady(bNewReadyState);
}

void APRPlayerController::StartRiftMission()
{
	ServerStartRiftMission();
}

void APRPlayerController::TryPickup()
{
	AActor* FocusedTarget = FindFocusedInteractionTarget();
	if (!FocusedTarget)
	{
		UE_LOG(LogProjectA, Verbose, TEXT("TryPickup found no nearby interactable for %s."), *GetNameSafe(this));
		return;
	}

	APRPickupActor* PickupCandidate = Cast<APRPickupActor>(FocusedTarget);
	if (PickupCandidate)
	{
		if (HasAuthority())
		{
			TryPickupOnServer(PickupCandidate);
		}
		else
		{
			ServerTryPickup(PickupCandidate);
		}
		return;
	}

	APRRiftObjectiveActor* ObjectiveCandidate = Cast<APRRiftObjectiveActor>(FocusedTarget);
	if (ObjectiveCandidate)
	{
		if (HasAuthority())
		{
			TryActivateRiftObjectiveOnServer(ObjectiveCandidate);
		}
		else
		{
			ServerTryActivateRiftObjective(ObjectiveCandidate);
		}
	}
}

void APRPlayerController::TryActivateRiftObjective()
{
	APRRiftObjectiveActor* ObjectiveCandidate = FindBestRiftObjectiveCandidate();
	if (!ObjectiveCandidate)
	{
		UE_LOG(LogProjectA, Verbose, TEXT("TryActivateRiftObjective found no nearby objective for %s."), *GetNameSafe(this));
		return;
	}

	if (HasAuthority())
	{
		TryActivateRiftObjectiveOnServer(ObjectiveCandidate);
	}
	else
	{
		ServerTryActivateRiftObjective(ObjectiveCandidate);
	}
}

void APRPlayerController::ToggleInventory()
{
	if (IsInventoryVisible())
	{
		HideInventory();
	}
	else
	{
		ShowInventory();
	}
}

void APRPlayerController::ShowInventory()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const bool bWasInventoryVisible = IsInventoryVisible();
	CreateInventoryUI();
	RefreshInventoryDisplay();

	if (InventoryWidget)
	{
		if (!bWasInventoryVisible)
		{
			bSavedMouseCursorVisibilityForInventory = bShowMouseCursor;
		}

		InventoryWidget->SetVisibility(ESlateVisibility::Visible);
		ApplyInventoryInputMode();
	}
}

void APRPlayerController::HideInventory()
{
	if (InventoryWidget)
	{
		InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	RestoreInventoryInputMode();
}

void APRPlayerController::RefreshInventoryDisplay()
{
	if (!IsLocalPlayerController() || !InventoryWidget)
	{
		return;
	}

	UPRInventoryComponent* InventoryComponent = GetLocalInventoryComponent();
	if (InventoryWidget->GetBoundInventory() != InventoryComponent)
	{
		InventoryWidget->BindInventory(InventoryComponent);
	}
	else
	{
		InventoryWidget->RefreshInventory();
	}
}

bool APRPlayerController::IsInventoryVisible() const
{
	return InventoryWidget && InventoryWidget->GetVisibility() == ESlateVisibility::Visible;
}

void APRPlayerController::UseInventoryItem(const FName ItemId)
{
	if (HasAuthority())
	{
		TryUseInventoryItemOnServer(ItemId);
		return;
	}

	ServerUseInventoryItem(ItemId);
}

void APRPlayerController::DropInventoryItem(const FName ItemId, const int32 Count)
{
	if (HasAuthority())
	{
		TryDropInventoryItemOnServer(ItemId, Count);
		return;
	}

	ServerDropInventoryItem(ItemId, Count);
}

void APRPlayerController::SpawnTestLoot()
{
	if (HasAuthority())
	{
		TrySpawnTestLootOnServer();
		return;
	}

	ServerSpawnTestLoot();
}

APRPickupActor* APRPlayerController::FindBestPickupCandidate() const
{
	const APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World || PickupInteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRPickupInteraction), false, ControlledPawn);
	QueryParams.AddIgnoredActor(ControlledPawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ControlledPawn->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(PickupInteractionRadius),
		QueryParams);

	APRPickupActor* BestPickup = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APRPickupActor* PickupActor = Cast<APRPickupActor>(Overlap.GetActor());
		if (!PickupActor || !PickupActor->CanBePickedUp())
		{
			continue;
		}

		if (InventoryComponent && !InventoryComponent->CanAddItem(PickupActor->GetItemInstance()))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ControlledPawn->GetActorLocation(), PickupActor->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestPickup = PickupActor;
		}
	}

	return BestPickup;
}

APRRiftObjectiveActor* APRPlayerController::FindBestRiftObjectiveCandidate() const
{
	const APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World || ObjectiveInteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PRRiftObjectiveInteraction), false, ControlledPawn);
	QueryParams.AddIgnoredActor(ControlledPawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ControlledPawn->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ObjectiveInteractionRadius),
		QueryParams);

	APRRiftObjectiveActor* BestObjective = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APRRiftObjectiveActor* ObjectiveActor = Cast<APRRiftObjectiveActor>(Overlap.GetActor());
		if (!ObjectiveActor || !ObjectiveActor->CanActivateObjective(const_cast<APRPlayerController*>(this)))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ControlledPawn->GetActorLocation(), ObjectiveActor->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestObjective = ObjectiveActor;
		}
	}

	return BestObjective;
}

AActor* APRPlayerController::FindFocusedInteractionTarget() const
{
	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	APRPickupActor* PickupCandidate = FindBestPickupCandidate();
	APRRiftObjectiveActor* ObjectiveCandidate = FindBestRiftObjectiveCandidate();
	if (!PickupCandidate && !ObjectiveCandidate)
	{
		return nullptr;
	}

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const float PickupDistanceSquared = PickupCandidate
		? FVector::DistSquared(PawnLocation, PickupCandidate->GetActorLocation())
		: TNumericLimits<float>::Max();
	const float ObjectiveDistanceSquared = ObjectiveCandidate
		? FVector::DistSquared(PawnLocation, ObjectiveCandidate->GetActorLocation())
		: TNumericLimits<float>::Max();

	return PickupDistanceSquared <= ObjectiveDistanceSquared
		? static_cast<AActor*>(PickupCandidate)
		: static_cast<AActor*>(ObjectiveCandidate);
}

bool APRPlayerController::IsFocusedInteractionTarget(const AActor* TargetActor) const
{
	return TargetActor && FindFocusedInteractionTarget() == TargetActor;
}

bool APRPlayerController::TryPickupOnServer(APRPickupActor* PickupActor)
{
	if (!HasAuthority())
	{
		ServerTryPickup(PickupActor);
		return false;
	}

	FString FailureReason;
	if (!CanServerPickup(PickupActor, &FailureReason))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Server pickup rejected. Controller=%s Pickup=%s Reason=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PickupActor),
			*FailureReason);
		return false;
	}

	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	if (!InventoryComponent)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Server pickup failed because inventory disappeared for %s."), *GetNameSafe(this));
		return false;
	}

	const FPRItemInstance PickedItem = PickupActor->GetItemInstance();
	if (!InventoryComponent->AddItem(PickedItem))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Server pickup rejected during inventory add. Controller=%s Pickup=%s ItemId=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PickupActor),
			*PickedItem.ItemId.ToString());
		return false;
	}

	PickupActor->SetPickedUp(true);
	PickupActor->Destroy();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Server pickup succeeded. Controller=%s Pickup=%s ItemId=%s Count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PickupActor),
		*PickedItem.ItemId.ToString(),
		PickedItem.Count);

	return true;
}

bool APRPlayerController::TryActivateRiftObjectiveOnServer(APRRiftObjectiveActor* ObjectiveActor)
{
	if (!HasAuthority())
	{
		ServerTryActivateRiftObjective(ObjectiveActor);
		return false;
	}

	FString FailureReason;
	if (!CanServerActivateRiftObjective(ObjectiveActor, &FailureReason))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Rift objective activation rejected. Controller=%s Objective=%s Reason=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ObjectiveActor),
			*FailureReason);
		return false;
	}

	const bool bActivated = ObjectiveActor->ActivateObjective(this);
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Rift objective activation %s. Controller=%s Objective=%s"),
		bActivated ? TEXT("succeeded") : TEXT("failed"),
		*GetNameSafe(this),
		*GetNameSafe(ObjectiveActor));
	return bActivated;
}

void APRPlayerController::ServerSetReady_Implementation(const bool bReady)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	if (!ProjectRiftPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ServerSetReady failed because APRPlayerState is missing."));
		return;
	}

	ProjectRiftPlayerState->SetReady(bReady);
	if (APRShipLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>() : nullptr)
	{
		LobbyGameMode->RefreshTeamMissionState();
	}
}

void APRPlayerController::ServerSetSelectedRoleModule_Implementation(const FName RoleModule)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	if (!ProjectRiftPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ServerSetSelectedRoleModule failed because APRPlayerState is missing."));
		return;
	}

	if (RoleModule.IsNone())
	{
		ProjectRiftPlayerState->SetSelectedRoleModule(NAME_None);
		return;
	}

	if (RoleModule == AssaultRoleModuleName || RoleModule == FName(TEXT("Role.Assault")))
	{
		ProjectRiftPlayerState->SetSelectedRoleModule(AssaultRoleModuleName);
		return;
	}

	UE_LOG(LogProjectA, Warning, TEXT("Unsupported role module requested: %s"), *RoleModule.ToString());
}

void APRPlayerController::ServerStartRiftMission_Implementation()
{
	UWorld* World = GetWorld();
	APRShipLobbyGameMode* LobbyGameMode = World ? Cast<APRShipLobbyGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!LobbyGameMode)
	{
		ClientMessage(TEXT("Rift mission can only be started from the ship lobby."));
		return;
	}

	LobbyGameMode->StartRiftMission(this);
}

void APRPlayerController::ServerTryPickup_Implementation(APRPickupActor* PickupActor)
{
	TryPickupOnServer(PickupActor);
}

void APRPlayerController::ServerTryActivateRiftObjective_Implementation(APRRiftObjectiveActor* ObjectiveActor)
{
	TryActivateRiftObjectiveOnServer(ObjectiveActor);
}

void APRPlayerController::ServerUseInventoryItem_Implementation(const FName ItemId)
{
	TryUseInventoryItemOnServer(ItemId);
}

void APRPlayerController::ServerDropInventoryItem_Implementation(const FName ItemId, const int32 Count)
{
	TryDropInventoryItemOnServer(ItemId, Count);
}

void APRPlayerController::ServerSpawnTestLoot_Implementation()
{
	TrySpawnTestLootOnServer();
}

void APRPlayerController::SubmitLocalMultiplayerProfile()
{
	if (!IsLocalPlayerController() || !GetPlayerState<APRPlayerState>() || !IsShipLobbyDebugWorld(GetWorld()))
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UPRSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		MultiplayerProfileStatus = TEXT("Save subsystem unavailable");
		return;
	}
	if (SaveSubsystem->HasPendingSettlementReceipt())
	{
		const FPRProfileOperationResult RetryResult = SaveSubsystem->RetryPendingSettlementReceipt();
		PersonalSettlementSaveStatus = RetryResult.IsSuccess() ? TEXT("Saved after retry") : TEXT("Retry failed");
		if (RiftSettlementWidget)
		{
			RiftSettlementWidget->SetPersonalSaveStatus(PersonalSettlementSaveStatus);
		}
		if (!RetryResult.IsSuccess())
		{
			MultiplayerProfileStatus = TEXT("Pending settlement must be saved before readying");
			ServerReportPendingSettlementSave();
			return;
		}
	}
	FPRMultiplayerProfileProjection Projection;
	FPRProfileOperationResult Result = SaveSubsystem->BuildMultiplayerProfileProjection(Projection);
	if (!Result.IsSuccess())
	{
		MultiplayerProfileStatus = Result.Diagnostic.IsEmpty() ? TEXT("No active profile") : Result.Diagnostic;
		return;
	}
	Result = SaveSubsystem->BindActiveProfileToSession();
	if (!Result.IsSuccess())
	{
		MultiplayerProfileStatus = Result.Diagnostic;
		return;
	}
	MultiplayerProfileStatus = TEXT("Binding");
	ServerBindMultiplayerProfile(Projection);
}

void APRPlayerController::ServerReportPendingSettlementSave_Implementation()
{
	if (APRShipLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>() : nullptr)
	{
		if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
		{
			ProjectRiftPlayerState->ClearMultiplayerProfileBinding();
		}
		LobbyGameMode->RefreshTeamMissionState();
	}
}

void APRPlayerController::ServerBindMultiplayerProfile_Implementation(const FPRMultiplayerProfileProjection& Projection)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	FString Diagnostic;
	if (!GetWorld() || !GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>())
	{
		ClientMultiplayerProfileBindingResult(false, TEXT("Profile binding is only accepted in the ship lobby."));
		return;
	}
	if (!ProjectRiftPlayerState || !Projection.IsValid(&Diagnostic))
	{
		ClientMultiplayerProfileBindingResult(false, Diagnostic.IsEmpty() ? TEXT("PlayerState or profile projection is invalid.") : Diagnostic);
		if (APRShipLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>())
		{
			LobbyGameMode->RefreshTeamMissionState();
		}
		return;
	}

	if (const AGameStateBase* CurrentGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		for (const TObjectPtr<APlayerState>& CandidateState : CurrentGameState->PlayerArray)
		{
			const APRPlayerState* Candidate = Cast<APRPlayerState>(CandidateState.Get());
			if (Candidate && Candidate != ProjectRiftPlayerState && Candidate->IsMultiplayerProfileBound()
				&& Candidate->GetBoundProfileId() == Projection.ProfileId)
			{
				Diagnostic = TEXT("This profile GUID is already bound to another player in the session.");
				ProjectRiftPlayerState->ClearMultiplayerProfileBinding();
				ClientMultiplayerProfileBindingResult(false, Diagnostic);
				if (APRShipLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>())
				{
					LobbyGameMode->RefreshTeamMissionState();
				}
				return;
			}
		}
	}

	if (!ProjectRiftPlayerState->BindMultiplayerProfile(Projection, Diagnostic))
	{
		ProjectRiftPlayerState->ClearMultiplayerProfileBinding();
		ClientMultiplayerProfileBindingResult(false, Diagnostic);
		if (APRShipLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>())
		{
			LobbyGameMode->RefreshTeamMissionState();
		}
		return;
	}
	ClientMultiplayerProfileBindingResult(true, TEXT("Profile bound"));
	if (APRShipLobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<APRShipLobbyGameMode>())
	{
		LobbyGameMode->RefreshTeamMissionState();
	}
}

void APRPlayerController::ClientMultiplayerProfileBindingResult_Implementation(const bool bAccepted, const FString& Diagnostic)
{
	MultiplayerProfileStatus = bAccepted ? TEXT("Bound") : Diagnostic;
	if (!bAccepted)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UPRSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UPRSaveSubsystem>())
			{
				SaveSubsystem->ReleaseSessionProfileBinding();
			}
		}
	}
}

void APRPlayerController::ClientReceivePersonalSettlement_Implementation(const FPRPlayerSettlementReceipt& Receipt)
{
	PersonalSettlementSaveStatus = TEXT("Saving");
	if (RiftSettlementWidget)
	{
		RiftSettlementWidget->SetPersonalSaveStatus(PersonalSettlementSaveStatus);
	}
	UGameInstance* GameInstance = GetGameInstance();
	UPRSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UPRSaveSubsystem>() : nullptr;
	const FPRProfileOperationResult Result = SaveSubsystem
		? SaveSubsystem->ApplyMultiplayerSettlementReceipt(Receipt)
		: FPRProfileOperationResult::MakeFailure(EPRProfileOperationStatus::IOError, TEXT("Save subsystem unavailable."), Receipt.ProfileId);
	if (Result.IsSuccess())
	{
		PersonalSettlementSaveStatus = Result.bAlreadyProcessedSettlement ? TEXT("Already saved") : TEXT("Saved");
	}
	else
	{
		PersonalSettlementSaveStatus = SaveSubsystem && SaveSubsystem->HasPendingSettlementReceipt()
			? TEXT("Retry in lobby")
			: TEXT("Save failed");
	}
	if (RiftSettlementWidget)
	{
		RiftSettlementWidget->SetPersonalSaveStatus(PersonalSettlementSaveStatus);
	}
	ServerAcknowledgePersonalSettlement(Receipt.SettlementId, Result.IsSuccess());
}

void APRPlayerController::ServerAcknowledgePersonalSettlement_Implementation(const FGuid SettlementId, const bool bSaveSucceeded)
{
	if (APRRiftGameMode* RiftGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		RiftGameMode->HandlePersonalSettlementAcknowledgement(this, SettlementId, bSaveSucceeded);
	}
}

bool APRPlayerController::TryUseInventoryItemOnServer(const FName ItemId)
{
	if (!HasAuthority())
	{
		ServerUseInventoryItem(ItemId);
		return false;
	}

	FString FailureReason;
	if (!CanUseInventoryItemOnServer(ItemId, &FailureReason))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory item use rejected. Controller=%s ItemId=%s Reason=%s"),
			*GetNameSafe(this),
			*ItemId.ToString(),
			*FailureReason);
		return false;
	}

	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	UPRAbilitySystemComponent* AbilitySystemComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	const TSubclassOf<UGameplayEffect> ConsumableEffectClass = ResolveConsumableEffectClass(ItemId);
	if (!AbilitySystemComponent || !InventoryComponent || !ConsumableEffectClass)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Inventory item use failed after validation for %s."), *ItemId.ToString());
		return false;
	}

	const UGameplayEffect* ConsumableEffect = ConsumableEffectClass->GetDefaultObject<UGameplayEffect>();
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	AbilitySystemComponent->ApplyGameplayEffectToSelf(ConsumableEffect, 1.0f, EffectContext);

	const bool bConsumed = InventoryComponent->UseItem(ItemId);
	if (!bConsumed)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Inventory item effect applied but item removal failed. ItemId=%s"), *ItemId.ToString());
		return false;
	}

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Inventory item used. Controller=%s ItemId=%s Remaining=%d Effect=%s"),
		*GetNameSafe(this),
		*ItemId.ToString(),
		InventoryComponent->GetItemCount(ItemId),
		*GetNameSafe(ConsumableEffect));

	return true;
}

bool APRPlayerController::TryDropInventoryItemOnServer(const FName ItemId, const int32 Count)
{
	if (!HasAuthority())
	{
		ServerDropInventoryItem(ItemId, Count);
		return false;
	}

	FString FailureReason;
	if (!CanDropInventoryItemOnServer(ItemId, Count, &FailureReason))
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Inventory drop rejected. Controller=%s ItemId=%s Count=%d Reason=%s"),
			*GetNameSafe(this),
			*ItemId.ToString(),
			Count,
			*FailureReason);
		return false;
	}

	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	FPRItemInstance DroppedItem;
	if (!InventoryComponent || !FindInventoryItemById(InventoryComponent, ItemId, DroppedItem))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Inventory drop failed after validation for %s."), *ItemId.ToString());
		return false;
	}

	DroppedItem.Count = Count;
	if (!InventoryComponent->RemoveItem(ItemId, Count))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Inventory drop failed during item removal. ItemId=%s Count=%d"), *ItemId.ToString(), Count);
		return false;
	}

	APRPickupActor* DroppedPickup = SpawnDroppedPickupOnServer(DroppedItem);
	if (!DroppedPickup)
	{
		InventoryComponent->AddItem(DroppedItem);
		UE_LOG(LogProjectA, Warning, TEXT("Inventory drop failed during pickup spawn; item restored. ItemId=%s Count=%d"), *ItemId.ToString(), Count);
		return false;
	}

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Inventory item dropped. Controller=%s Pickup=%s ItemId=%s Count=%d Remaining=%d"),
		*GetNameSafe(this),
		*GetNameSafe(DroppedPickup),
		*ItemId.ToString(),
		Count,
		InventoryComponent->GetItemCount(ItemId));

	return true;
}

APRPickupActor* APRPlayerController::TrySpawnTestLootOnServer(const float RollOverride)
{
	if (!HasAuthority())
	{
		ServerSpawnTestLoot();
		return nullptr;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Test loot spawn failed for %s: pawn is missing."), *GetNameSafe(this));
		return nullptr;
	}

	UPRLootTableDataAsset* LootTableToUse = TestLootTable;
	if (!LootTableToUse)
	{
		LootTableToUse = NewObject<UPRLootTableDataAsset>(this);
	}

	if (!LootTableToUse || !LootTableToUse->IsValidLootTable())
	{
		UE_LOG(LogProjectA, Warning, TEXT("Test loot spawn failed for %s: loot table is invalid."), *GetNameSafe(this));
		return nullptr;
	}

	FVector Forward = ControlledPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector LootSpawnLocation = ControlledPawn->GetActorLocation()
		+ Forward * FMath::Max(0.0f, DropForwardDistance)
		+ FVector(0.0f, 0.0f, DropHeightOffset);

	TSubclassOf<APRPickupActor> ResolvedPickupActorClass = PickupActorClass;
	if (!ResolvedPickupActorClass)
	{
		ResolvedPickupActorClass = APRPickupActor::StaticClass();
	}

	APRPickupActor* LootPickup = UPRLootTableLibrary::SpawnLootPickupFromTable(
		this,
		LootTableToUse,
		ResolvedPickupActorClass,
		LootSpawnLocation,
		FRotator::ZeroRotator,
		RollOverride);

	if (LootPickup)
	{
		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Test loot spawned. Controller=%s Pickup=%s ItemId=%s"),
			*GetNameSafe(this),
			*GetNameSafe(LootPickup),
			*LootPickup->GetItemInstance().ItemId.ToString());
	}

	return LootPickup;
}

void APRPlayerController::RefreshLobbyReadyDebugDisplay()
{
#if UE_BUILD_SHIPPING
	return;
#endif
	if (!IsLocalPlayerController())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!IsShipLobbyDebugWorld(World))
	{
		DestroyLobbyReadyDebugHUD();
		return;
	}

	CreateLobbyReadyDebugHUD();

	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
#if !UE_BUILD_SHIPPING
		if (LobbyReadyDebugWidget)
		{
			LobbyReadyDebugWidget->SetReadyText(FText::FromString(TEXT("Lobby Ready\nWaiting for game state...")));
		}
#endif
		return;
	}

	const APRGameState* ProjectRiftGameState = Cast<APRGameState>(GameState);
	const FName TeamMissionId = ProjectRiftGameState ? ProjectRiftGameState->GetSelectedTeamMissionId() : NAME_None;
	const UPRMissionProgressionDataAsset* Mission = UPRAssetManager::Get()
		? UPRAssetManager::Get()->LoadMissionSync(TeamMissionId)
		: nullptr;
	const APRPlayerState* LocalPlayerState = GetPlayerState<APRPlayerState>();
	const bool bLocalEligible = Mission && LocalPlayerState && Mission->IsEligible(LocalPlayerState->GetBoundStoryProgress());
	bool bAllBound = true;
	bool bAllReady = true;
	for (const APlayerState* ListedPlayerState : GameState->PlayerArray)
	{
		const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(ListedPlayerState);
		bAllBound &= ProjectRiftPlayerState && ProjectRiftPlayerState->IsMultiplayerProfileBound();
		bAllReady &= ProjectRiftPlayerState && ProjectRiftPlayerState->IsReady();
	}
	FString BlockReason = TEXT("None");
	if (!Mission || !Mission->IsContractValid())
	{
		BlockReason = TEXT("Mission contract unavailable");
	}
	else if (!LocalPlayerState || !LocalPlayerState->IsMultiplayerProfileBound())
	{
		BlockReason = TEXT("Local profile is not bound");
	}
	else if (HasAuthority() && !bLocalEligible)
	{
		BlockReason = TEXT("Host is not eligible for mission");
	}
	else if (!bAllBound)
	{
		BlockReason = TEXT("Waiting for all profile bindings");
	}
	else if (!bAllReady)
	{
		BlockReason = TEXT("Waiting for all players to ready");
	}

	FString ReadyList = FString::Printf(
		TEXT("Lobby Team Status\nMission: %s | Contract: %s | Can start: %s\nProfile: %s | Host eligible: %s\nStart blocked by: %s\n1: Assault | P: Ready | Host O: Start Rift\n"),
		TeamMissionId.IsNone() ? TEXT("None") : *TeamMissionId.ToString(),
		Mission && Mission->IsContractValid() ? TEXT("VALID") : TEXT("INVALID"),
		ProjectRiftGameState && ProjectRiftGameState->IsTeamMissionReady() ? TEXT("YES") : TEXT("NO"),
		*MultiplayerProfileStatus,
		HasAuthority() ? (bLocalEligible ? TEXT("YES") : TEXT("NO")) : TEXT("HOST ONLY"),
		*BlockReason);
	for (const APlayerState* ListedPlayerState : GameState->PlayerArray)
	{
		ReadyList += GetLobbyReadyLine(ListedPlayerState);
		ReadyList += LINE_TERMINATOR;
	}

	if (LobbyReadyDebugWidget)
	{
		LobbyReadyDebugWidget->SetReadyText(FText::FromString(ReadyList));
	}
}

void APRPlayerController::CreateGASDebugHUD()
{
	if (!IsLocalPlayerController() || GASDebugWidget)
	{
		return;
	}

	if (!GASDebugWidgetClass)
	{
		GASDebugWidgetClass = UPRGASDebugWidget::StaticClass();
	}

	GASDebugWidget = CreateWidget<UPRGASDebugWidget>(this, GASDebugWidgetClass);
	if (GASDebugWidget)
	{
		GASDebugWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		GASDebugWidget->AddToPlayerScreen(20);
		GASDebugWidget->SetPositionInViewport(FPRDebugUILayout::GASPosition(), false);
		GASDebugWidget->SetDesiredSizeInViewport(FPRDebugUILayout::GASSize());
		GASDebugWidget->SetAnchorsInViewport(FPRDebugUILayout::GASAnchors());
		GASDebugWidget->SetAlignmentInViewport(FPRDebugUILayout::GASAlignment());
	}
}

void APRPlayerController::DestroyGASDebugHUD()
{
	if (GASDebugWidget)
	{
		GASDebugWidget->RemoveFromParent();
		GASDebugWidget = nullptr;
	}
}

void APRPlayerController::CreateLobbyReadyDebugHUD()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!IsLocalPlayerController() || LobbyReadyDebugWidget)
	{
		return;
	}

	LobbyReadyDebugWidget = CreateWidget<UPRLobbyReadyDebugWidget>(this, UPRLobbyReadyDebugWidget::StaticClass());
	if (LobbyReadyDebugWidget)
	{
		LobbyReadyDebugWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		LobbyReadyDebugWidget->AddToPlayerScreen(21);
		LobbyReadyDebugWidget->SetPositionInViewport(FPRDebugUILayout::LobbyReadyPosition(), false);
		LobbyReadyDebugWidget->SetDesiredSizeInViewport(FPRDebugUILayout::LobbyReadySize());
		LobbyReadyDebugWidget->SetAnchorsInViewport(FPRDebugUILayout::LobbyReadyAnchors());
		LobbyReadyDebugWidget->SetAlignmentInViewport(FPRDebugUILayout::LobbyReadyAlignment());
	}
#endif
}

void APRPlayerController::DestroyLobbyReadyDebugHUD()
{
	if (LobbyReadyDebugWidget)
	{
		LobbyReadyDebugWidget->RemoveFromParent();
		LobbyReadyDebugWidget = nullptr;
	}
}

void APRPlayerController::CreateInventoryUI()
{
	if (!IsLocalPlayerController() || InventoryWidget)
	{
		return;
	}

	if (!InventoryWidgetClass)
	{
		InventoryWidgetClass = UPRInventoryWidget::StaticClass();
	}

	InventoryWidget = CreateWidget<UPRInventoryWidget>(this, InventoryWidgetClass);
	if (InventoryWidget)
	{
		InventoryWidget->OnUseItemRequested.AddUniqueDynamic(this, &APRPlayerController::HandleInventoryUseRequested);
		InventoryWidget->OnDropItemRequested.AddUniqueDynamic(this, &APRPlayerController::HandleInventoryDropRequested);
		InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		InventoryWidget->AddToPlayerScreen(30);
		InventoryWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
		InventoryWidget->SetDesiredSizeInViewport(FVector2D(560.0, 540.0));
		InventoryWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
		InventoryWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		RefreshInventoryDisplay();
	}
}

void APRPlayerController::DestroyInventoryUI()
{
	RestoreInventoryInputMode();

	if (InventoryWidget)
	{
		InventoryWidget->OnUseItemRequested.RemoveDynamic(this, &APRPlayerController::HandleInventoryUseRequested);
		InventoryWidget->OnDropItemRequested.RemoveDynamic(this, &APRPlayerController::HandleInventoryDropRequested);
		InventoryWidget->RemoveFromParent();
		InventoryWidget = nullptr;
	}
}

void APRPlayerController::CreateRiftSettlementUI()
{
	if (!IsLocalPlayerController() || RiftSettlementWidget)
	{
		return;
	}

	if (!RiftSettlementWidgetClass)
	{
		RiftSettlementWidgetClass = UPRRiftSettlementWidget::StaticClass();
	}

	RiftSettlementWidget = CreateWidget<UPRRiftSettlementWidget>(this, RiftSettlementWidgetClass);
	if (RiftSettlementWidget)
	{
		RiftSettlementWidget->SetPersonalSaveStatus(PersonalSettlementSaveStatus);
		RiftSettlementWidget->SetVisibility(ESlateVisibility::Collapsed);
		RiftSettlementWidget->AddToPlayerScreen(45);
		RiftSettlementWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
		RiftSettlementWidget->SetDesiredSizeInViewport(FVector2D(640.0, 420.0));
		RiftSettlementWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
		RiftSettlementWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	}
}

void APRPlayerController::DestroyRiftSettlementUI()
{
	if (RiftSettlementWidget)
	{
		RiftSettlementWidget->RemoveFromParent();
		RiftSettlementWidget = nullptr;
	}
}

void APRPlayerController::ApplyInventoryInputMode()
{
	if (!IsLocalPlayerController() || !InventoryWidget)
	{
		return;
	}

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	bInventoryInputModeActive = true;
}

void APRPlayerController::RestoreInventoryInputMode()
{
	if (!IsLocalPlayerController() || !bInventoryInputModeActive)
	{
		return;
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	bShowMouseCursor = bSavedMouseCursorVisibilityForInventory;
	bInventoryInputModeActive = false;
}

UPRInventoryComponent* APRPlayerController::GetLocalInventoryComponent() const
{
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	return ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
}

TSubclassOf<UGameplayEffect> APRPlayerController::ResolveConsumableEffectClass(const FName ItemId) const
{
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	const UPRItemDataAsset* ItemData = InventoryComponent ? InventoryComponent->FindItemData(ItemId) : nullptr;
	if (ItemData && ItemData->bCanUse && ItemData->UseEffect)
	{
		return ItemData->UseEffect;
	}

	if (ItemId == HealthInjectorItemId)
	{
		return HealthInjectorEffectClass;
	}

	if (ItemId == ShieldPackItemId)
	{
		return ShieldPackEffectClass;
	}

	return nullptr;
}

bool APRPlayerController::CanUseInventoryItemOnServer(const FName ItemId, FString* OutFailureReason) const
{
	auto Reject = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!HasAuthority())
	{
		return Reject(TEXT("controller is not authoritative"));
	}

	if (ItemId.IsNone())
	{
		return Reject(TEXT("item id is empty"));
	}

	APRCharacter* ProjectRiftCharacter = Cast<APRCharacter>(GetPawn());
	if (!ProjectRiftCharacter)
	{
		return Reject(TEXT("controlled ProjectRift character is missing"));
	}

	if (!ProjectRiftCharacter->IsAlive())
	{
		return Reject(TEXT("character is not alive"));
	}

	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	const UPRAttributeSet* AttributeSet = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetAttributeSet() : nullptr;
	const UPRAbilitySystemComponent* AbilitySystemComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!InventoryComponent)
	{
		return Reject(TEXT("inventory is missing"));
	}

	if (InventoryComponent->GetItemCount(ItemId) <= 0)
	{
		return Reject(TEXT("item is not in inventory"));
	}

	if (!AttributeSet)
	{
		return Reject(TEXT("attribute set is missing"));
	}

	if (!AbilitySystemComponent)
	{
		return Reject(TEXT("ability system component is missing"));
	}

	const TSubclassOf<UGameplayEffect> ConsumableEffectClass = ResolveConsumableEffectClass(ItemId);
	if (!ConsumableEffectClass)
	{
		return Reject(TEXT("item is not a supported consumable"));
	}

	if (ItemId == HealthInjectorItemId || ConsumableEffectClass->IsChildOf(UPRHealthConsumableGameplayEffect::StaticClass()))
	{
		if (AttributeSet->GetHealth() >= AttributeSet->GetMaxHealth())
		{
			return Reject(TEXT("health is already full"));
		}
	}
	else if (ItemId == ShieldPackItemId || ConsumableEffectClass->IsChildOf(UPRShieldConsumableGameplayEffect::StaticClass()))
	{
		if (AttributeSet->GetShield() >= AttributeSet->GetMaxShield())
		{
			return Reject(TEXT("shield is already full"));
		}
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}

bool APRPlayerController::CanDropInventoryItemOnServer(const FName ItemId, const int32 Count, FString* OutFailureReason) const
{
	auto Reject = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!HasAuthority())
	{
		return Reject(TEXT("controller is not authoritative"));
	}

	if (ItemId.IsNone())
	{
		return Reject(TEXT("item id is empty"));
	}

	if (Count <= 0)
	{
		return Reject(TEXT("drop count must be positive"));
	}

	if (!GetWorld())
	{
		return Reject(TEXT("world is missing"));
	}

	if (!GetPawn())
	{
		return Reject(TEXT("controller has no pawn"));
	}

	if (!PickupActorClass)
	{
		return Reject(TEXT("pickup actor class is missing"));
	}

	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	if (!InventoryComponent)
	{
		return Reject(TEXT("inventory is missing"));
	}

	const int32 AvailableCount = InventoryComponent->GetItemCount(ItemId);
	if (AvailableCount <= 0)
	{
		return Reject(TEXT("item is not in inventory"));
	}

	if (AvailableCount < Count)
	{
		return Reject(TEXT("drop count exceeds inventory count"));
	}

	FPRItemInstance ExistingItem;
	if (!FindInventoryItemById(InventoryComponent, ItemId, ExistingItem) || !ExistingItem.IsValid())
	{
		return Reject(TEXT("inventory item is invalid"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}

APRPickupActor* APRPlayerController::SpawnDroppedPickupOnServer(const FPRItemInstance& DroppedItem) const
{
	if (!HasAuthority() || !DroppedItem.IsValid())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn || !PickupActorClass)
	{
		return nullptr;
	}

	FVector Forward = ControlledPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector DropLocation = ControlledPawn->GetActorLocation()
		+ Forward * FMath::Max(0.0f, DropForwardDistance)
		+ FVector(0.0f, 0.0f, DropHeightOffset);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = const_cast<APRPlayerController*>(this);
	SpawnParameters.Instigator = ControlledPawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APRPickupActor* DroppedPickup = World->SpawnActor<APRPickupActor>(PickupActorClass, DropLocation, FRotator::ZeroRotator, SpawnParameters);
	if (DroppedPickup)
	{
		DroppedPickup->SetItemInstance(DroppedItem);
	}

	return DroppedPickup;
}

void APRPlayerController::HandleInventoryUseRequested(const FName ItemId)
{
	UseInventoryItem(ItemId);
}

void APRPlayerController::HandleInventoryDropRequested(const FName ItemId, const int32 Count)
{
	DropInventoryItem(ItemId, Count);
}

bool APRPlayerController::CanServerPickup(APRPickupActor* PickupActor, FString* OutFailureReason) const
{
	auto Reject = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!HasAuthority())
	{
		return Reject(TEXT("controller is not authoritative"));
	}

	if (!PickupActor)
	{
		return Reject(TEXT("pickup is null"));
	}

	if (PickupActor->IsActorBeingDestroyed())
	{
		return Reject(TEXT("pickup is already being destroyed"));
	}

	if (PickupActor->GetWorld() != GetWorld())
	{
		return Reject(TEXT("pickup belongs to a different world"));
	}

	if (!PickupActor->CanBePickedUp())
	{
		return Reject(TEXT("pickup is not available"));
	}

	const FPRItemInstance PickedItem = PickupActor->GetItemInstance();
	if (!PickedItem.IsValid())
	{
		return Reject(TEXT("pickup item is invalid"));
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return Reject(TEXT("controller has no pawn"));
	}

	const float MaxDistance = FMath::Max(0.0f, PickupInteractionRadius);
	if (FVector::DistSquared(ControlledPawn->GetActorLocation(), PickupActor->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return Reject(TEXT("pickup is out of range"));
	}

	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRInventoryComponent* InventoryComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetInventoryComponent() : nullptr;
	if (!InventoryComponent)
	{
		return Reject(TEXT("inventory is missing"));
	}

	if (!InventoryComponent->CanAddItem(PickedItem))
	{
		return Reject(TEXT("inventory cannot accept item"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}

bool APRPlayerController::CanServerActivateRiftObjective(APRRiftObjectiveActor* ObjectiveActor, FString* OutFailureReason) const
{
	auto Reject = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!HasAuthority())
	{
		return Reject(TEXT("controller is not authoritative"));
	}

	if (!ObjectiveActor)
	{
		return Reject(TEXT("objective is null"));
	}

	if (ObjectiveActor->IsActorBeingDestroyed())
	{
		return Reject(TEXT("objective is being destroyed"));
	}

	if (ObjectiveActor->GetWorld() != GetWorld())
	{
		return Reject(TEXT("objective belongs to a different world"));
	}

	if (!ObjectiveActor->CanActivateObjective(const_cast<APRPlayerController*>(this)))
	{
		return Reject(TEXT("objective is not available"));
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return Reject(TEXT("controller has no pawn"));
	}

	const float MaxDistance = FMath::Max(ObjectiveInteractionRadius, ObjectiveActor->GetInteractionRadius());
	if (FVector::DistSquared(ControlledPawn->GetActorLocation(), ObjectiveActor->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return Reject(TEXT("objective is out of range"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return true;
}
