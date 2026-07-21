#include "Bosses/PRBossTelegraphActor.h"

#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"

APRBossTelegraphActor::APRBossTelegraphActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	WarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarningLight"));
	SetRootComponent(WarningLight);
	WarningLight->SetIntensity(2200.0f);
	WarningLight->SetAttenuationRadius(550.0f);
	WarningLight->SetLightColor(FLinearColor(1.0f, 0.15f, 0.03f));
	WarningText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WarningText"));
	WarningText->SetupAttachment(WarningLight);
	WarningText->SetHorizontalAlignment(EHTA_Center);
	WarningText->SetWorldSize(34.0f);
	WarningText->SetTextRenderColor(FColor(255, 74, 20));
	WarningText->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
}

void APRBossTelegraphActor::InitializeTelegraph(const FPRBossTelegraphDefinition& InDefinition, const FName PatternId)
{
	if (!HasAuthority()) { return; }
	const FLinearColor Color = InDefinition.Color.GetClamped(0.0f, 1.0f);
	WarningLight->SetLightColor(Color);
	WarningLight->SetAttenuationRadius(FMath::Max(200.0f, InDefinition.Radius > 0.0f ? InDefinition.Radius : 550.0f));
	WarningText->SetTextRenderColor(Color.ToFColor(true));
	WarningText->SetText(FText::FromString(FString::Printf(TEXT("! %s !"), *PatternId.ToString())));
	SetLifeSpan(FMath::Max(0.2f, InDefinition.DurationSeconds + 0.2f));
}
