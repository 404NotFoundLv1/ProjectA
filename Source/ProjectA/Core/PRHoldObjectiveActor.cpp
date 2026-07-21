#include "Core/PRHoldObjectiveActor.h"

#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "Settings/PRProjectSettings.h"

APRHoldObjectiveActor::APRHoldObjectiveActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APRHoldObjectiveActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsObjectiveActive())
	{
		return;
	}

	const float SafeHoldDuration = GetHoldDuration();
	SetCurrentHoldTime(CurrentHoldTime + DeltaSeconds);
	SetObjectiveProgress(CurrentHoldTime / SafeHoldDuration);

	if (CurrentHoldTime >= SafeHoldDuration)
	{
		CompleteObjective();
	}
}

float APRHoldObjectiveActor::GetHoldDuration() const
{
	if (HoldDurationOverride > 0.0f)
	{
		return FMath::Max(0.1f, HoldDurationOverride);
	}
	const UPRProjectSettings* ProjectSettings = GetDefault<UPRProjectSettings>();
	if (!ProjectSettings)
	{
		UE_LOG(LogProjectA, Error, TEXT("ProjectRift project settings are unavailable while reading the hold objective duration; using the code default."));
	}
	return ProjectSettings
		? FMath::Max(0.1f, ProjectSettings->ObjectiveHoldDuration)
		: 30.0f;
}

void APRHoldObjectiveActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRHoldObjectiveActor, CurrentHoldTime);
}

void APRHoldObjectiveActor::HandleObjectiveActivated(AController* ActivatingController)
{
	SetCurrentHoldTime(0.0f);
	SetObjectiveProgress(0.0f);
}

void APRHoldObjectiveActor::OnRep_CurrentHoldTime()
{
}

void APRHoldObjectiveActor::SetCurrentHoldTime(const float InCurrentHoldTime)
{
	CurrentHoldTime = FMath::Clamp(InCurrentHoldTime, 0.0f, GetHoldDuration());
}
