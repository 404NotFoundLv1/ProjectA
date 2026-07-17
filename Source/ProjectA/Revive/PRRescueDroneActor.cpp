#include "Revive/PRRescueDroneActor.h"

#include "Characters/PRCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Revive/PRReviveComponent.h"
#include "Settings/PRProjectSettings.h"
#include "UObject/ConstructorHelpers.h"

APRRescueDroneActor::APRRescueDroneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetActorEnableCollision(false);

	DroneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneMesh"));
	SetRootComponent(DroneMesh);
	DroneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DroneMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.18f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DroneMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void APRRescueDroneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APRRescueDroneActor, DroneState);
	DOREPLIFETIME(APRRescueDroneActor, AssignedPlayer);
	DOREPLIFETIME(APRRescueDroneActor, RescueTarget);
}

bool APRRescueDroneActor::InitializeForPlayer(APRCharacter* InOwnerCharacter)
{
	if (!HasAuthority() || !InOwnerCharacter || DroneState != EPRRescueDroneState::Unavailable)
	{
		return false;
	}
	AssignedPlayer = InOwnerCharacter;
	DroneState = EPRRescueDroneState::Ready;
	SetActorLocation(InOwnerCharacter->GetActorLocation() + FVector(-160.0f, 90.0f, 130.0f));
	ForceNetUpdate();
	return true;
}

bool APRRescueDroneActor::RequestRescue(APRCharacter* DownedCharacter)
{
	if (!HasAuthority() || DroneState != EPRRescueDroneState::Ready || !DownedCharacter || DownedCharacter != AssignedPlayer || !DownedCharacter->IsDowned())
	{
		return false;
	}
	RescueTarget = DownedCharacter;
	DroneState = EPRRescueDroneState::Deployed;
	RescueStartServerTime = 0.0f;
	ForceNetUpdate();
	return true;
}

void APRRescueDroneActor::ShutdownDrone()
{
	if (HasAuthority())
	{
		Destroy();
	}
}

void APRRescueDroneActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (DroneState == EPRRescueDroneState::Ready)
	{
		if (!AssignedPlayer || !AssignedPlayer->IsAlive())
		{
			return;
		}
		const FVector FollowLocation = AssignedPlayer->GetActorLocation() + FVector(-160.0f, 90.0f, 130.0f);
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), FollowLocation, DeltaSeconds, 8.0f));
		return;
	}
	if (DroneState != EPRRescueDroneState::Deployed)
	{
		return;
	}
	if (!RescueTarget || !RescueTarget->IsDowned() || !RescueTarget->GetReviveComponent())
	{
		DroneState = EPRRescueDroneState::Spent;
		RescueTarget = nullptr;
		ForceNetUpdate();
		return;
	}
	const FVector TargetLocation = RescueTarget->GetActorLocation() + FVector(0.0f, 0.0f, 115.0f);
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, 12.0f));
	if (FVector::DistSquared(GetActorLocation(), TargetLocation) > FMath::Square(110.0f))
	{
		return;
	}
	UPRReviveComponent* Revive = RescueTarget->GetReviveComponent();
	if (RescueStartServerTime <= 0.0f)
	{
		if (!Revive->BeginDroneRevive())
		{
			DroneState = EPRRescueDroneState::Spent;
			RescueTarget = nullptr;
			ForceNetUpdate();
			return;
		}
		RescueStartServerTime = GetWorld()->GetTimeSeconds();
		return;
	}
	// The revive component owns the authoritative duration and completion.  The drone
	// only owns the one-use mission resource and follows that state to completion.
	if (!RescueTarget->IsDowned() || !Revive->IsReviveInProgress())
	{
		DroneState = EPRRescueDroneState::Spent;
		RescueTarget = nullptr;
		ForceNetUpdate();
	}
}
