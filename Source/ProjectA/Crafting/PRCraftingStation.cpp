#include "Crafting/PRCraftingStation.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

APRCraftingStation::APRCraftingStation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CraftingStationMesh"));
	SetRootComponent(Mesh);
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CraftingInteractionSphere"));
	InteractionSphere->SetupAttachment(Mesh);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

FText APRCraftingStation::GetInteractionPromptText() const
{
	return FText::FromString(TEXT("Craft / Dismantle / Upgrade"));
}
