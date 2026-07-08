#include "Core/PRExtractionZone.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/PRInteractionPromptWidget.h"
#include "UObject/ConstructorHelpers.h"

APRExtractionZone::APRExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ZoneMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMarkerMesh"));
	ZoneMarkerMesh->SetupAttachment(SceneRoot);
	ZoneMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMarkerMesh->SetGenerateOverlapEvents(false);
	ZoneMarkerMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ZoneMarkerMesh->SetStaticMesh(CylinderMesh.Object);
	}

	ExtractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ExtractionSphere"));
	ExtractionSphere->SetupAttachment(SceneRoot);
	ExtractionSphere->SetSphereRadius(ExtractionRadius);
	ExtractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	ExtractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExtractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExtractionSphere->SetGenerateOverlapEvents(true);
	ExtractionSphere->OnComponentBeginOverlap.AddDynamic(this, &APRExtractionZone::HandleExtractionSphereBeginOverlap);
	ExtractionSphere->OnComponentEndOverlap.AddDynamic(this, &APRExtractionZone::HandleExtractionSphereEndOverlap);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(SceneRoot);
	InteractionPromptWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawSize(FVector2D(520.0f, 76.0f));
	InteractionPromptWidget->SetPivot(FVector2D(0.5f, 0.5f));
	InteractionPromptWidget->SetWidgetClass(UPRInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPromptWidget->SetGenerateOverlapEvents(false);
	InteractionPromptWidget->SetHiddenInGame(true);
	InteractionPromptWidget->SetVisibility(false, true);

	const float CylinderRadius = 50.0f;
	const float MarkerScale = FMath::Max(ExtractionRadius / CylinderRadius, 0.1f);
	ZoneMarkerMesh->SetRelativeScale3D(FVector(MarkerScale, MarkerScale, 0.04f));
	ZoneMarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
}

void APRExtractionZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (ExtractionSphere)
	{
		ExtractionSphere->SetSphereRadius(ExtractionRadius);
	}

	if (ZoneMarkerMesh)
	{
		const float CylinderRadius = 50.0f;
		const float MarkerScale = FMath::Max(ExtractionRadius / CylinderRadius, 0.1f);
		ZoneMarkerMesh->SetRelativeScale3D(FVector(MarkerScale, MarkerScale, 0.04f));
		ZoneMarkerMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	}
}

void APRExtractionZone::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TryExtractNearbyPawns();
	}

	UpdateInteractionPromptVisibility();
}

bool APRExtractionZone::CanExtractPawn(APawn* ExtractingPawn) const
{
	if (!ExtractingPawn)
	{
		return false;
	}

	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || !RiftGameState->IsExtractionAvailable() || RiftGameState->IsExtractionComplete())
	{
		return false;
	}

	const float PromptRadius = ExtractionSphere
		? FMath::Max(ExtractionRadius, ExtractionSphere->GetUnscaledSphereRadius())
		: ExtractionRadius;
	const float ExtractionRadiusSquared = FMath::Square(FMath::Max(PromptRadius, 1.0f));
	if (FVector::DistSquared(ExtractingPawn->GetActorLocation(), GetActorLocation()) > ExtractionRadiusSquared)
	{
		return false;
	}

	AController* ExtractingController = ExtractingPawn->GetController();
	APRRiftGameMode* RiftGameMode = GetRiftGameMode();
	return RiftGameMode && ExtractingController && !RiftGameMode->IsPlayerExtracted(ExtractingController);
}

bool APRExtractionZone::TryExtractPawn(APawn* ExtractingPawn)
{
	if (!HasAuthority() || !CanExtractPawn(ExtractingPawn))
	{
		return false;
	}

	APRRiftGameMode* RiftGameMode = GetRiftGameMode();
	AController* ExtractingController = ExtractingPawn ? ExtractingPawn->GetController() : nullptr;
	const bool bExtracted = RiftGameMode && RiftGameMode->RegisterPlayerExtracted(ExtractingController);
	if (bExtracted)
	{
		RefreshInteractionPromptWidget();
	}

	return bExtracted;
}

FText APRExtractionZone::GetInteractionPromptText() const
{
	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState)
	{
		return FText::FromString(TEXT("\u64A4\u79BB\u70B9\u5F85\u547D"));
	}

	if (RiftGameState->IsExtractionComplete())
	{
		return FText::FromString(TEXT("\u64A4\u79BB\u5B8C\u6210"));
	}

	if (RiftGameState->IsExtractionAvailable())
	{
		return FText::FromString(TEXT("\u8FDB\u5165\u64A4\u79BB\u533A\u64A4\u79BB"));
	}

	return FText::FromString(TEXT("\u5B8C\u6210\u4E3B\u76EE\u6807\u540E\u5F00\u542F\u64A4\u79BB"));
}

void APRExtractionZone::SetInteractionPromptVisible(const bool bVisible)
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

void APRExtractionZone::RefreshInteractionPromptWidget()
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
	PromptWidget->SetPromptColor(GetInteractionPromptColor());
}

FLinearColor APRExtractionZone::GetInteractionPromptColor() const
{
	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (RiftGameState && RiftGameState->IsExtractionComplete())
	{
		return FLinearColor(0.25f, 1.0f, 0.38f, 1.0f);
	}

	if (RiftGameState && RiftGameState->IsExtractionAvailable())
	{
		return FLinearColor(0.2f, 1.0f, 1.0f, 1.0f);
	}

	return FLinearColor(1.0f, 0.82f, 0.2f, 1.0f);
}

void APRExtractionZone::UpdateInteractionPromptVisibility()
{
	APawn* NearbyPawn = FindNearbyPromptPawn();
	const bool bShouldShowPrompt = NearbyPawn != nullptr;
	if (bShouldShowPrompt)
	{
		RefreshInteractionPromptWidget();
	}

	SetInteractionPromptVisible(bShouldShowPrompt);
}

APawn* APRExtractionZone::FindNearbyPromptPawn() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float PromptRadius = ExtractionSphere
		? FMath::Max(ExtractionRadius, ExtractionSphere->GetUnscaledSphereRadius())
		: ExtractionRadius;
	const float PromptRadiusSquared = FMath::Square(FMath::Max(PromptRadius, 1.0f));
	const FVector ZoneLocation = GetActorLocation();

	auto IsPawnInPromptRange = [&ZoneLocation, PromptRadiusSquared](const APawn* Pawn)
	{
		return Pawn && FVector::DistSquared(Pawn->GetActorLocation(), ZoneLocation) <= PromptRadiusSquared;
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

void APRExtractionZone::TryExtractNearbyPawns()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const APRRiftGameState* RiftGameState = GetRiftGameState();
	if (!RiftGameState || !RiftGameState->IsExtractionAvailable() || RiftGameState->IsExtractionComplete())
	{
		return;
	}

	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		TryExtractPawn(*PawnIt);
	}
}

APRRiftGameMode* APRExtractionZone::GetRiftGameMode() const
{
	UWorld* World = GetWorld();
	return World ? Cast<APRRiftGameMode>(World->GetAuthGameMode()) : nullptr;
}

APRRiftGameState* APRExtractionZone::GetRiftGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<APRRiftGameState>() : nullptr;
}

void APRExtractionZone::HandleExtractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* ExtractingPawn = Cast<APawn>(OtherActor);
	if (!ExtractingPawn)
	{
		return;
	}

	RefreshInteractionPromptWidget();
	SetInteractionPromptVisible(true);

	if (HasAuthority())
	{
		TryExtractPawn(ExtractingPawn);
	}
}

void APRExtractionZone::HandleExtractionSphereEndOverlap(
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
