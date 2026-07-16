#include "Deployables/PRDeployableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "TimerManager.h"

APRDeployableActor::APRDeployableActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SetRootComponent(MeshComponent);
	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(MeshComponent);
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetTextRenderColor(FColor::White);
}

void APRDeployableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APRDeployableActor, DeployableKind);
	DOREPLIFETIME(APRDeployableActor, OwningPlayerState);
	DOREPLIFETIME(APRDeployableActor, ExpireAtServerTime);
}

void APRDeployableActor::InitializeDeployable(APRPlayerState* InOwnerPlayerState, const EPRDeployableKind InKind, const float InLifetimeSeconds)
{
	if (!HasAuthority() || !FMath::IsFinite(InLifetimeSeconds) || InLifetimeSeconds <= 0.0f)
	{
		return;
	}
	OwningPlayerState = InOwnerPlayerState;
	SetOwner(InOwnerPlayerState ? InOwnerPlayerState->GetPlayerController() : nullptr);
	DeployableKind = InKind;
	ExpireAtServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() + InLifetimeSeconds : 0.0f;
	SetLifeSpan(InLifetimeSeconds);
	ForceNetUpdate();
}

float APRDeployableActor::GetRemainingLifetimeSeconds() const
{
	return GetWorld() ? FMath::Max(0.0f, ExpireAtServerTime - GetWorld()->GetTimeSeconds()) : 0.0f;
}
