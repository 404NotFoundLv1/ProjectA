#include "Items/PRPickupActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "UObject/ConstructorHelpers.h"

APRPickupActor::APRPickupActor()
	: bIsPickedUp(false)
{
	PrimaryActorTick.bCanEverTick = false;
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
}
