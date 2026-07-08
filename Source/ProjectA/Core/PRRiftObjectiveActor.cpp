#include "Core/PRRiftObjectiveActor.h"

#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRRiftGameMode.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerController.h"
#include "UI/PRInteractionPromptWidget.h"

APRRiftObjectiveActor::APRRiftObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &APRRiftObjectiveActor::HandleInteractionSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &APRRiftObjectiveActor::HandleInteractionSphereEndOverlap);

	InteractionPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptWidget"));
	InteractionPromptWidget->SetupAttachment(SceneRoot);
	InteractionPromptWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 320.0f));
	InteractionPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionPromptWidget->SetDrawSize(FVector2D(420.0f, 76.0f));
	InteractionPromptWidget->SetPivot(FVector2D(0.5f, 0.5f));
	InteractionPromptWidget->SetWidgetClass(UPRInteractionPromptWidget::StaticClass());
	InteractionPromptWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPromptWidget->SetGenerateOverlapEvents(false);
	InteractionPromptWidget->SetHiddenInGame(true);
	InteractionPromptWidget->SetVisibility(false, true);
}

void APRRiftObjectiveActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateInteractionPromptVisibility();
}

void APRRiftObjectiveActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRRiftObjectiveActor, ObjectiveState);
	DOREPLIFETIME(APRRiftObjectiveActor, ObjectiveProgress);
}

bool APRRiftObjectiveActor::ActivateObjective(AController* ActivatingController)
{
	if (!HasAuthority() || !CanActivateObjective(ActivatingController))
	{
		return false;
	}

	SetObjectiveState(EPRRiftObjectiveState::Active);
	SetObjectiveProgress(0.0f);

	if (APRRiftGameMode* RiftGameMode = GetRiftGameMode())
	{
		RiftGameMode->HandleObjectiveActivated(this);
	}

	HandleObjectiveActivated(ActivatingController);
	return true;
}

bool APRRiftObjectiveActor::CanActivateObjective(AController* ActivatingController) const
{
	return ObjectiveState == EPRRiftObjectiveState::NotStarted;
}

void APRRiftObjectiveActor::CompleteObjective()
{
	if (!HasAuthority() || IsObjectiveCompleted())
	{
		return;
	}

	SetObjectiveProgress(1.0f);
	SetObjectiveState(EPRRiftObjectiveState::Completed);

	if (APRRiftGameMode* RiftGameMode = GetRiftGameMode())
	{
		RiftGameMode->HandleObjectiveCompleted(this);
	}

	HandleObjectiveCompleted();
}

FText APRRiftObjectiveActor::GetInteractionPromptText() const
{
	switch (ObjectiveState)
	{
	case EPRRiftObjectiveState::Active:
		return FText::FromString(FString::Printf(TEXT("\u4EFB\u52A1\u8FDB\u884C\u4E2D %.0f%%"), ObjectiveProgress * 100.0f));
	case EPRRiftObjectiveState::Completed:
		return FText::FromString(TEXT("\u4EFB\u52A1\u5B8C\u6210"));
	case EPRRiftObjectiveState::NotStarted:
	default:
		return FText::FromString(TEXT("\u6309 F \u5F00\u59CB\u4EFB\u52A1"));
	}
}

void APRRiftObjectiveActor::HandleObjectiveActivated(AController* ActivatingController)
{
}

void APRRiftObjectiveActor::HandleObjectiveCompleted()
{
}

void APRRiftObjectiveActor::SetObjectiveState(const EPRRiftObjectiveState InObjectiveState)
{
	ObjectiveState = InObjectiveState;
	RefreshInteractionPromptText();
}

void APRRiftObjectiveActor::SetObjectiveProgress(const float InObjectiveProgress)
{
	ObjectiveProgress = FMath::Clamp(InObjectiveProgress, 0.0f, 1.0f);
	RefreshInteractionPromptText();

	if (HasAuthority())
	{
		if (APRRiftGameMode* RiftGameMode = GetRiftGameMode())
		{
			RiftGameMode->HandleObjectiveProgressChanged(this, ObjectiveProgress);
		}
	}
}

APRRiftGameMode* APRRiftObjectiveActor::GetRiftGameMode() const
{
	UWorld* World = GetWorld();
	return World ? Cast<APRRiftGameMode>(World->GetAuthGameMode()) : nullptr;
}

void APRRiftObjectiveActor::OnRep_ObjectiveState()
{
	RefreshInteractionPromptText();
}

void APRRiftObjectiveActor::OnRep_ObjectiveProgress()
{
	RefreshInteractionPromptText();
}

void APRRiftObjectiveActor::RefreshInteractionPromptText()
{
	RefreshInteractionPromptWidget();
}

void APRRiftObjectiveActor::SetInteractionPromptVisible(const bool bVisible)
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

void APRRiftObjectiveActor::RefreshInteractionPromptWidget()
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

FLinearColor APRRiftObjectiveActor::GetInteractionPromptColor() const
{
	switch (ObjectiveState)
	{
	case EPRRiftObjectiveState::Active:
		return FLinearColor(1.0f, 0.82f, 0.2f, 1.0f);
	case EPRRiftObjectiveState::Completed:
		return FLinearColor(0.25f, 1.0f, 0.38f, 1.0f);
	case EPRRiftObjectiveState::NotStarted:
	default:
		return FLinearColor(0.2f, 1.0f, 1.0f, 1.0f);
	}
}

void APRRiftObjectiveActor::HandleInteractionSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	UpdateInteractionPromptVisibility();
}

void APRRiftObjectiveActor::HandleInteractionSphereEndOverlap(
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

void APRRiftObjectiveActor::UpdateInteractionPromptVisibility()
{
	APawn* NearbyPawn = FindNearbyPromptPawn();
	const bool bShouldShowPrompt = NearbyPawn != nullptr;

	if (bShouldShowPrompt)
	{
		RefreshInteractionPromptText();
	}

	SetInteractionPromptVisible(bShouldShowPrompt);
}

APawn* APRRiftObjectiveActor::FindNearbyPromptPawn() const
{
	const UWorld* World = GetWorld();
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
