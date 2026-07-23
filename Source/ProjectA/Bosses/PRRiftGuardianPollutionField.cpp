#include "Bosses/PRRiftGuardianPollutionField.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRStatusGameplayEffect.h"
#include "Bosses/PRBossCharacter.h"
#include "Characters/PRCharacter.h"
#include "Components/SphereComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

APRRiftGuardianPollutionField::APRRiftGuardianPollutionField()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	RadiusComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PollutionRadius"));
	SetRootComponent(RadiusComponent);
	RadiusComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APRRiftGuardianPollutionField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PulseTimer);
	Super::EndPlay(EndPlayReason);
}

void APRRiftGuardianPollutionField::InitializeField(APRBossCharacter* InSourceBoss, const float InRadius, const float InDurationSeconds, const float InMagnitude)
{
	if (!HasAuthority() || !InSourceBoss)
	{
		return;
	}
	SourceBoss = InSourceBoss;
	Radius = FMath::Max(1.0f, InRadius);
	Magnitude = FMath::Max(0.0f, InMagnitude);
	RadiusComponent->SetSphereRadius(Radius);
	SetLifeSpan(FMath::Max(0.1f, InDurationSeconds));
	PulsePollution();
	GetWorldTimerManager().SetTimer(PulseTimer, this, &APRRiftGuardianPollutionField::PulsePollution, 1.0f, true);
}

void APRRiftGuardianPollutionField::PulsePollution()
{
	APRBossCharacter* Boss = SourceBoss.Get();
	UPRAbilitySystemComponent* SourceASC = Boss ? Boss->GetProjectRiftAbilitySystemComponent() : nullptr;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Boss || !SourceASC || !GameState || Magnitude <= 0.0f)
	{
		return;
	}
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APRCharacter* Target = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
		if (!Target || !Target->IsAlive() || FVector::DistSquared(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(Radius))
		{
			continue;
		}
		if (UPRAbilitySystemComponent* TargetASC = Target->GetProjectRiftAbilitySystemComponent())
		{
			UPRCombatEffectLibrary::ApplyStatusEffectToTarget(
				SourceASC,
				TargetASC,
				FPRTargetStatusEffectDefinition(UPRPollutionStatusGameplayEffect::StaticClass(), Magnitude, 1.05f),
				Boss);
		}
	}
}
