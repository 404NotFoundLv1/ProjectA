#include "Weapons/PRWeaponActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APRWeaponActor::APRWeaponActor()
{
	bReplicates = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ReceiverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Receiver"));
	ReceiverMesh->SetupAttachment(SceneRoot);
	ReceiverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReceiverMesh->SetRelativeLocation(FVector(18.0, 0.0, 0.0));
	ReceiverMesh->SetRelativeScale3D(FVector(0.45, 0.07, 0.10));

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
	BarrelMesh->SetupAttachment(SceneRoot);
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrelMesh->SetRelativeLocation(FVector(52.0, 0.0, 2.0));
	BarrelMesh->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	BarrelMesh->SetRelativeScale3D(FVector(0.035, 0.035, 0.35));

	GripMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grip"));
	GripMesh->SetupAttachment(SceneRoot);
	GripMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GripMesh->SetRelativeLocation(FVector(5.0, 0.0, -12.0));
	GripMesh->SetRelativeRotation(FRotator(0.0, 0.0, -18.0));
	GripMesh->SetRelativeScale3D(FVector(0.08, 0.06, 0.18));

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(SceneRoot);
	Muzzle->SetRelativeLocation(FVector(88.0, 0.0, 2.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeMesh.Succeeded())
	{
		ReceiverMesh->SetStaticMesh(CubeMesh.Object);
		GripMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		BarrelMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

FTransform APRWeaponActor::GetMuzzleTransform() const
{
	return Muzzle ? Muzzle->GetComponentTransform() : GetActorTransform();
}
