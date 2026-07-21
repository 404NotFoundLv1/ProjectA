#include "Items/PRPickupActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerController.h"
#include "ProjectA.h"
#include "UI/PRInteractionPromptWidget.h"
#include "UObject/ConstructorHelpers.h"

APRPickupActor::APRPickupActor()
	: bIsPickedUp(false)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetRelativeScale3D(FVector(0.35f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(Mesh);
	InteractionSphere->SetSphereRadius(350.0f);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &APRPickupActor::HandleInteractionSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &APRPickupActor::HandleInteractionSphereEndOverlap);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(Mesh);
	InteractionPromptWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawSize(FVector2D(360.0f, 72.0f));
	InteractionPromptWidget->SetPivot(FVector2D(0.5f, 0.5f));
	InteractionPromptWidget->SetWidgetClass(UPRInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPromptWidget->SetGenerateOverlapEvents(false);
	InteractionPromptWidget->SetHiddenInGame(true);
	InteractionPromptWidget->SetVisibility(false, true);
}

void APRPickupActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateInteractionPromptVisibility();
}

void APRPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRPickupActor, ItemInstance);
	DOREPLIFETIME(APRPickupActor, RewardSource);
	DOREPLIFETIME(APRPickupActor, ObjectiveNodeId);
	DOREPLIFETIME(APRPickupActor, bIsPickedUp);
}

void APRPickupActor::SetItemInstance(const FPRItemInstance& InItemInstance)
{
	ItemInstance = InItemInstance;
	if (HasAuthority() && ItemInstance.IsValid() && !ItemInstance.HasValidIdentity())
	{
		ItemInstance.InstanceGuid = FGuid::NewGuid();
	}
	ForceNetUpdate();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Pickup item set. Pickup=%s ItemId=%s Count=%d"),
		*GetNameSafe(this),
		*ItemInstance.ItemId.ToString(),
		ItemInstance.Count);
}

void APRPickupActor::SetRewardSource(const FPRRewardSourceContext& InRewardSource)
{
	if (!HasAuthority())
	{
		return;
	}
	RewardSource = InRewardSource;
	ForceNetUpdate();
}

bool APRPickupActor::CanBePickedUp() const
{
	return ItemInstance.IsValid() && !bIsPickedUp;
}

FText APRPickupActor::GetInteractionPromptText() const
{
	return FText::FromString(TEXT("\u6309 F \u62FE\u53D6"));
}

void APRPickupActor::SetPickedUp(const bool bNewPickedUp)
{
	if (bIsPickedUp == bNewPickedUp)
	{
		return;
	}

	bIsPickedUp = bNewPickedUp;
	RefreshPickupVisualState();
	ForceNetUpdate();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Pickup state changed. Pickup=%s ItemId=%s bIsPickedUp=%s"),
		*GetNameSafe(this),
		*ItemInstance.ItemId.ToString(),
		bIsPickedUp ? TEXT("true") : TEXT("false"));
}

void APRPickupActor::OnRep_ItemInstance()
{
	RefreshPickupVisualState();
}

void APRPickupActor::OnRep_IsPickedUp()
{
	RefreshPickupVisualState();
}

void APRPickupActor::HandleInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	UpdateInteractionPromptVisibility();
}

void APRPickupActor::HandleInteractionSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex)
{
	if (Cast<APawn>(OtherActor))
	{
		UpdateInteractionPromptVisibility();
	}
}

void APRPickupActor::RefreshPickupVisualState()
{
	if (Mesh)
	{
		Mesh->SetVisibility(!bIsPickedUp, true);
		Mesh->SetHiddenInGame(bIsPickedUp);
	}

	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(bIsPickedUp ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
		InteractionSphere->SetGenerateOverlapEvents(!bIsPickedUp);
	}

	if (bIsPickedUp || !CanBePickedUp())
	{
		SetInteractionPromptVisible(false);
	}
}

void APRPickupActor::SetInteractionPromptVisible(const bool bVisible)
{
	if (!InteractionPromptWidget)
	{
		return;
	}

	if (bVisible)
	{
		RefreshInteractionPromptWidget();
	}

	InteractionPromptWidget->SetHiddenInGame(!bVisible);
	InteractionPromptWidget->SetVisibility(bVisible, true);
}

void APRPickupActor::RefreshInteractionPromptWidget()
{
	if (!InteractionPromptWidget)
	{
		return;
	}

	InteractionPromptWidget->InitWidget();
	UPRInteractionPromptWidget* PromptWidget = Cast<UPRInteractionPromptWidget>(InteractionPromptWidget->GetUserWidgetObject());
	if (!PromptWidget)
	{
		return;
	}

	PromptWidget->SetPromptText(GetInteractionPromptText());
	PromptWidget->SetPromptColor(FLinearColor(1.0f, 0.86f, 0.24f, 1.0f));
}

void APRPickupActor::UpdateInteractionPromptVisibility()
{
	APawn* NearbyPawn = FindNearbyPromptPawn();
	const bool bShouldShowPrompt = NearbyPawn != nullptr;

	if (NearbyPawn)
	{
		UpdateInteractionPromptPlacement(NearbyPawn);
	}

	SetInteractionPromptVisible(bShouldShowPrompt);
}

APawn* APRPickupActor::FindNearbyPromptPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APRPlayerController* LocalPlayerController = nullptr;
	for (TActorIterator<APRPlayerController> ControllerIt(World); ControllerIt; ++ControllerIt)
	{
		APRPlayerController* CandidateController = *ControllerIt;
		if (CandidateController && CandidateController->GetLocalPlayer() && CandidateController->GetPawn())
		{
			LocalPlayerController = CandidateController;
			break;
		}
	}

	if (!LocalPlayerController)
	{
		for (TActorIterator<APRPlayerController> ControllerIt(World); ControllerIt; ++ControllerIt)
		{
			APRPlayerController* CandidateController = *ControllerIt;
			if (CandidateController && CandidateController->IsLocalController() && CandidateController->GetPawn())
			{
				LocalPlayerController = CandidateController;
				break;
			}
		}
	}

	if (!LocalPlayerController)
	{
		for (TActorIterator<APRPlayerController> ControllerIt(World); ControllerIt; ++ControllerIt)
		{
			APRPlayerController* CandidateController = *ControllerIt;
			if (CandidateController && CandidateController->GetPawn())
			{
				LocalPlayerController = CandidateController;
				break;
			}
		}
	}

	return LocalPlayerController && LocalPlayerController->IsFocusedInteractionTarget(this)
		? LocalPlayerController->GetPawn()
		: nullptr;
}

void APRPickupActor::UpdateInteractionPromptPlacement(const APawn* NearbyPawn)
{
	if (!InteractionPromptWidget)
	{
		return;
	}

	constexpr float PromptVerticalOffset = 45.0f;
	constexpr float MinHorizontalOffset = 30.0f;
	constexpr float MaxHorizontalOffset = 90.0f;

	FVector PromptLocation = GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
	if (Mesh)
	{
		Mesh->UpdateBounds();
		const FBoxSphereBounds MeshBounds = Mesh->Bounds;
		PromptLocation = MeshBounds.Origin;
		PromptLocation.Z = MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z + PromptVerticalOffset;

		if (NearbyPawn)
		{
			FVector DirectionToPlayer = NearbyPawn->GetActorLocation() - MeshBounds.Origin;
			DirectionToPlayer.Z = 0.0f;
			if (DirectionToPlayer.Normalize())
			{
				const float HorizontalOffset = FMath::Clamp(MeshBounds.SphereRadius * 0.35f, MinHorizontalOffset, MaxHorizontalOffset);
				PromptLocation += DirectionToPlayer * HorizontalOffset;
			}
		}
	}

	InteractionPromptWidget->SetWorldLocation(PromptLocation);
}
