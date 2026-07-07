#include "Items/PRPickupActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
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
	InteractionSphere->SetSphereRadius(180.0f);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &APRPickupActor::HandleInteractionSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &APRPickupActor::HandleInteractionSphereEndOverlap);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(Mesh);
	InteractionPromptWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
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
	DOREPLIFETIME(APRPickupActor, bIsPickedUp);
}

void APRPickupActor::SetItemInstance(const FPRItemInstance& InItemInstance)
{
	ItemInstance = InItemInstance;
	ForceNetUpdate();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Pickup item set. Pickup=%s ItemId=%s Count=%d"),
		*GetNameSafe(this),
		*ItemInstance.ItemId.ToString(),
		ItemInstance.Count);
}

bool APRPickupActor::CanBePickedUp() const
{
	return ItemInstance.IsValid() && !bIsPickedUp;
}

FText APRPickupActor::GetInteractionPromptText() const
{
	return FText::FromString(TEXT("按 F 拾取"));
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
	if (!Cast<APawn>(OtherActor) || !CanBePickedUp())
	{
		return;
	}

	SetInteractionPromptVisible(true);
}

void APRPickupActor::HandleInteractionSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex)
{
	if (Cast<APawn>(OtherActor))
	{
		SetInteractionPromptVisible(false);
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
	APawn* NearbyPawn = CanBePickedUp() ? FindNearbyPromptPawn() : nullptr;
	const bool bShouldShowPrompt = NearbyPawn != nullptr;

	SetInteractionPromptVisible(bShouldShowPrompt);
}

APawn* APRPickupActor::FindNearbyPromptPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float PromptRadius = InteractionSphere
		? FMath::Max(InteractionSphere->GetUnscaledSphereRadius(), 1.0f)
		: 180.0f;
	const float PromptRadiusSquared = FMath::Square(PromptRadius);
	const FVector PickupLocation = GetActorLocation();

	auto IsPawnInPromptRange = [&PickupLocation, PromptRadiusSquared](const APawn* Pawn)
	{
		return Pawn && FVector::DistSquared(Pawn->GetActorLocation(), PickupLocation) <= PromptRadiusSquared;
	};

	if (const APlayerController* LocalPlayerController = World->GetFirstPlayerController())
	{
		APawn* LocalPawn = LocalPlayerController->GetPawn();
		return IsPawnInPromptRange(LocalPawn) ? LocalPawn : nullptr;
	}

	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		APawn* CandidatePawn = *PawnIt;
		if (IsPawnInPromptRange(CandidatePawn))
		{
			return CandidatePawn;
		}
	}

	return nullptr;
}
