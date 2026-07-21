#include "Bosses/PRBossCharacter.h"

#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Bosses/PRBossDefinitionDataAsset.h"
#include "Bosses/PRBossEncounterController.h"
#include "Bosses/PRBossSchedulerComponent.h"
#include "Bosses/PRBossWeakPointComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayAbilitySpec.h"

APRBossCharacter::APRBossCharacter()
{
	EnemyMeleeAbilityClass = nullptr;
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::Disabled;
	BossScheduler = CreateDefaultSubobject<UPRBossSchedulerComponent>(TEXT("BossScheduler"));
	DefaultWeakPoint = CreateDefaultSubobject<UPRBossWeakPointComponent>(TEXT("BossWeakPoint"));
	WeakPointCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BossWeakPointCollision"));
	WeakPointCollision->SetupAttachment(GetCapsuleComponent());
	WeakPointCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 76.0f));
	WeakPointCollision->SetSphereRadius(42.0f);
	WeakPointCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WeakPointCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeakPointCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	for (int32 Index = 1; Index < 4; ++Index)
	{
		const FName ComponentName(*FString::Printf(TEXT("BossWeakPoint%d"), Index));
		UPRBossWeakPointComponent* WeakPoint = CreateDefaultSubobject<UPRBossWeakPointComponent>(ComponentName);
		AdditionalWeakPoints.Add(WeakPoint);
		USphereComponent* Collision = CreateDefaultSubobject<USphereComponent>(*FString::Printf(TEXT("BossWeakPointCollision%d"), Index));
		Collision->SetupAttachment(GetCapsuleComponent());
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
		AdditionalWeakPointCollisions.Add(Collision);
	}
}

const UPRBossWeakPointComponent* APRBossCharacter::FindWeakPoint(const UPrimitiveComponent* HitComponent) const
{
	if (HitComponent == WeakPointCollision.Get()) { return DefaultWeakPoint; }
	for (int32 Index = 0; Index < AdditionalWeakPointCollisions.Num(); ++Index)
	{
		if (HitComponent == AdditionalWeakPointCollisions[Index].Get()) { return AdditionalWeakPoints.IsValidIndex(Index) ? AdditionalWeakPoints[Index] : nullptr; }
	}
	return nullptr;
}

bool APRBossCharacter::IsWeakPointHitComponent(const UPrimitiveComponent* HitComponent) const
{
	const UPRBossWeakPointComponent* WeakPoint = FindWeakPoint(HitComponent);
	return WeakPoint && !WeakPoint->GetWeakPointId().IsNone();
}

float APRBossCharacter::GetWeakPointDamageMultiplier(const UPrimitiveComponent* HitComponent) const
{
	const UPRBossWeakPointComponent* WeakPoint = FindWeakPoint(HitComponent);
	return WeakPoint && IsWeakPointExposed() ? WeakPoint->GetDamageMultiplier() : 1.0f;
}

bool APRBossCharacter::IsWeakPointExposed() const
{
	return BossScheduler && BossScheduler->IsWeakPointExposed();
}

void APRBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || !BossDefinition || !AttributeSet)
	{
		return;
	}

	const FPRBossAttributeDefinition& Attributes = BossDefinition->Attributes;
	const float HealthMultiplier = BossDefinition->HealthScaling.GetForPlayerCount(FrozenPlayerCount);
	AttributeSet->SetMaxHealth(FMath::Max(1.0f, Attributes.MaxHealth * HealthMultiplier));
	AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
	AttributeSet->SetMaxShield(Attributes.MaxShield);
	AttributeSet->SetShield(FMath::Min(Attributes.InitialShield, Attributes.MaxShield));
	AttributeSet->SetAttackPower(Attributes.AttackPower * BossDefinition->DamageScaling.GetForPlayerCount(FrozenPlayerCount));
	AttributeSet->SetMoveSpeed(Attributes.MoveSpeed);
	GetCharacterMovement()->MaxWalkSpeed = Attributes.MoveSpeed;
	for (const FPRBossAbilityPatternDefinition& Pattern : BossDefinition->AbilityPatterns)
	{
		if (Pattern.AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(Pattern.AbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Pattern.AbilityClass, 1));
		}
	}
	BossScheduler->StartScheduler(this, BossDefinition, FrozenPlayerCount);
}

bool APRBossCharacter::InitializeBoss(UPRBossDefinitionDataAsset* InDefinition, APRBossEncounterController* InEncounterController, const int32 InFrozenPlayerCount)
{
	if (!HasAuthority() || HasActorBegunPlay() || !InDefinition)
	{
		return false;
	}

	FString Diagnostic;
	if (!InDefinition->ValidateDefinition(Diagnostic))
	{
		return false;
	}

	BossDefinition = InDefinition;
	BossDefinitionId = InDefinition->BossId;
	EncounterController = InEncounterController;
	FrozenPlayerCount = FMath::Clamp(InFrozenPlayerCount, 1, 4);
	if (!InDefinition->WeakPoints.IsEmpty())
	{
		DefaultWeakPoint->Configure(InDefinition->WeakPoints[0]);
		WeakPointCollision->SetRelativeLocation(InDefinition->WeakPoints[0].RelativeLocation);
		WeakPointCollision->SetSphereRadius(InDefinition->WeakPoints[0].Radius);
		for (int32 Index = 0; Index < AdditionalWeakPoints.Num(); ++Index)
		{
			const int32 DefinitionIndex = Index + 1;
			USphereComponent* Collision = AdditionalWeakPointCollisions.IsValidIndex(Index) ? AdditionalWeakPointCollisions[Index] : nullptr;
			if (InDefinition->WeakPoints.IsValidIndex(DefinitionIndex) && Collision)
			{
				AdditionalWeakPoints[Index]->Configure(InDefinition->WeakPoints[DefinitionIndex]);
				Collision->SetRelativeLocation(InDefinition->WeakPoints[DefinitionIndex].RelativeLocation);
				Collision->SetSphereRadius(InDefinition->WeakPoints[DefinitionIndex].Radius);
				Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			}
		}
	}
	return !BossDefinitionId.IsNone();
}

void APRBossCharacter::HandleCombatUnitDamageResolved(
	AActor* DamageSource,
	const float ResolvedDamage,
	const FGameplayEffectContextHandle& EffectContext)
{
	Super::HandleCombatUnitDamageResolved(DamageSource, ResolvedDamage);
	if (!HasAuthority() || ResolvedDamage <= 0.0f || !BossScheduler)
	{
		return;
	}
	const FHitResult* HitResult = EffectContext.GetHitResult();
	if (HitResult && IsWeakPointHitComponent(HitResult->GetComponent()))
	{
		BossScheduler->NotifyWeakPointDamage(FindWeakPoint(HitResult->GetComponent())->GetWeakPointId(), ResolvedDamage);
	}
}

void APRBossCharacter::HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext)
{
	Super::HandleCombatUnitHealthDepleted(EffectContext);
	if (APRBossEncounterController* BossEncounter = EncounterController.Get())
	{
		BossEncounter->NotifyBossDefeated(this);
	}
}

void APRBossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APRBossCharacter, BossDefinitionId);
}
