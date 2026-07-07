#include "Player/PRPlayerController.h"

#include "Core/PRShipLobbyGameMode.h"
#include "Engine/OverlapResult.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRPickupActor.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "UI/PRGASDebugWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName AssaultRoleModuleName(TEXT("Ability.Role.Assault"));

FString GetLobbyReadyLine(const APlayerState* PlayerState)
{
	const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(PlayerState);
	const bool bReady = ProjectRiftPlayerState && ProjectRiftPlayerState->IsReady();
	const FName SelectedRoleModule = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetSelectedRoleModule() : NAME_None;
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
		TEXT("P%d %s: %s | Role: %s"),
		PlayerId,
		*DisplayName,
		bReady ? TEXT("READY") : TEXT("WAITING"),
		SelectedRoleModule.IsNone() ? TEXT("None") : *SelectedRoleModule.ToString());
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
	}
}

void APRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &APRPlayerController::TryPickup);
	}
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
	APRPickupActor* PickupCandidate = FindBestPickupCandidate();
	if (!PickupCandidate)
	{
		UE_LOG(LogProjectA, Verbose, TEXT("TryPickup found no nearby pickup for %s."), *GetNameSafe(this));
		return;
	}

	ServerTryPickup(PickupCandidate);
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
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APRPickupActor* PickupActor = Cast<APRPickupActor>(Overlap.GetActor());
		if (!PickupActor || !PickupActor->CanBePickedUp())
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

void APRPlayerController::ServerSetReady_Implementation(const bool bReady)
{
	APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	if (!ProjectRiftPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ServerSetReady failed because APRPlayerState is missing."));
		return;
	}

	ProjectRiftPlayerState->SetReady(bReady);
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

void APRPlayerController::RefreshLobbyReadyDebugDisplay()
{
	if (!IsLocalPlayerController() || !GEngine)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	FString ReadyList = TEXT("Lobby Ready\n");
	ReadyList += TEXT("1: Assault | P: Ready | Host O: Start Rift\n");
	for (const APlayerState* ListedPlayerState : GameState->PlayerArray)
	{
		ReadyList += GetLobbyReadyLine(ListedPlayerState);
		ReadyList += LINE_TERMINATOR;
	}

	const int32 MessageKey = IsLocalController() ? 16016 : 16017;
	GEngine->AddOnScreenDebugMessage(MessageKey, 1.1f, FColor::Green, ReadyList);
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
		GASDebugWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
		GASDebugWidget->SetPositionInViewport(FVector2D(16.0, 16.0), false);
		GASDebugWidget->SetDesiredSizeInViewport(FVector2D(420.0, 220.0));
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
