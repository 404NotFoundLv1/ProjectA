#include "Enemies/PREnemyCharacter.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRDamageGameplayEffect.h"
#include "Animation/AnimInstance.h"
#include "Characters/PRCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRRiftGameMode.h"
#include "Enemies/PREnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Items/PRLootTableDataAsset.h"
#include "Items/PRLootTableLibrary.h"
#include "Items/PRPickupActor.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "UI/PREnemyHealthBarWidget.h"
#include "UObject/ConstructorHelpers.h"

APREnemyCharacter::APREnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	AIControllerClass = APREnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCharacterMovement()->MaxWalkSpeed = 360.0f;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetRelativeScale3D(FVector(1.08f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> EnemyMeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (EnemyMeshAsset.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(EnemyMeshAsset.Object);
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> EnemyAnimClass(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (EnemyAnimClass.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(EnemyAnimClass.Class);
	}

	EnemyHealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHealthBarWidget"));
	EnemyHealthBarWidget->SetupAttachment(GetRootComponent());
	EnemyHealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EnemyHealthBarWidget->SetDrawAtDesiredSize(false);
	EnemyHealthBarWidget->SetDrawSize(FVector2D(220.0f, 58.0f));
	EnemyHealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
	EnemyHealthBarWidget->SetWidgetClass(UPREnemyHealthBarWidget::StaticClass());
	EnemyHealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);

	PickupActorClass = APRPickupActor::StaticClass();
}

void APREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = FMath::Clamp(Health <= 0.0f ? MaxHealth : Health, 0.0f, MaxHealth);
	}

	InitializeHealthBarWidget();
}

void APREnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APREnemyCharacter, Health);
	DOREPLIFETIME(APREnemyCharacter, bDead);
}

float APREnemyCharacter::TakeDamage(
	const float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	return ApplyEnemyDamage(DamageAmount, EventInstigator) ? DamageAmount : 0.0f;
}

float APREnemyCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

bool APREnemyCharacter::ApplyEnemyDamage(const float DamageAmount, AController* DamageInstigator)
{
	if (!HasAuthority() || bDead || DamageAmount <= 0.0f)
	{
		return false;
	}

	Health = FMath::Clamp(Health - DamageAmount, 0.0f, MaxHealth);
	if (Health <= 0.0f)
	{
		HandleDeath(DamageInstigator);
	}

	return true;
}

bool APREnemyCharacter::TryMeleeAttack(AActor* TargetActor)
{
	if (!HasAuthority() || bDead || !TargetActor)
	{
		return false;
	}

	if (FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) > FMath::Square(AttackRange))
	{
		return false;
	}

	return ApplyDamageToProjectRiftCharacter(TargetActor);
}

APRPickupActor* APREnemyCharacter::SpawnDeathLoot()
{
	if (!HasAuthority() || bDeathLootSpawned)
	{
		return nullptr;
	}

	UPRLootTableDataAsset* LootTableToUse = DeathLootTable.Get();
	if (!LootTableToUse)
	{
		LootTableToUse = NewObject<UPRLootTableDataAsset>(this);
	}

	APRPickupActor* LootPickup = UPRLootTableLibrary::SpawnLootPickupFromTable(
		this,
		LootTableToUse,
		PickupActorClass,
		GetActorLocation() + FVector(0.0f, 0.0f, 28.0f),
		FRotator::ZeroRotator,
		DeathLootRollOverride);

	bDeathLootSpawned = LootPickup != nullptr;
	return LootPickup;
}

void APREnemyCharacter::HandleDeath(AController* DeathInstigator)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	Health = 0.0f;
	ApplyDeathState();
	SpawnDeathLoot();

	if (UWorld* World = GetWorld())
	{
		if (APRRiftGameMode* RiftGameMode = World->GetAuthGameMode<APRRiftGameMode>())
		{
			RiftGameMode->RegisterEnemyKilled(this, DeathInstigator);
		}
	}

	UE_LOG(LogProjectA, Log, TEXT("Enemy died. Enemy=%s Instigator=%s"), *GetNameSafe(this), *GetNameSafe(DeathInstigator));
}

void APREnemyCharacter::ApplyDeathState()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetVisibility(false, true);
	}

	if (EnemyHealthBarWidget)
	{
		EnemyHealthBarWidget->SetHiddenInGame(true);
	}
}

bool APREnemyCharacter::ApplyDamageToProjectRiftCharacter(AActor* TargetActor)
{
	APRCharacter* TargetCharacter = Cast<APRCharacter>(TargetActor);
	if (!TargetCharacter || !TargetCharacter->IsAlive())
	{
		return false;
	}

	UPRAbilitySystemComponent* TargetASC = TargetCharacter->GetProjectRiftAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	const FGameplayTag DamageTag = ProjectRiftGameplayTags::Data_Damage;
	if (!DamageTag.IsValid())
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(this, this);
	EffectContext.AddActors({ TargetCharacter }, true);

	FGameplayEffectSpecHandle DamageSpecHandle = TargetASC->MakeOutgoingSpec(
		UPRDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return false;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageTag, AttackDamage);
	TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
	return true;
}

void APREnemyCharacter::InitializeHealthBarWidget()
{
	if (!EnemyHealthBarWidget)
	{
		return;
	}

	EnemyHealthBarWidget->InitWidget();
	if (UPREnemyHealthBarWidget* HealthBar = Cast<UPREnemyHealthBarWidget>(EnemyHealthBarWidget->GetUserWidgetObject()))
	{
		HealthBar->SetObservedEnemy(this);
	}
}

void APREnemyCharacter::OnRep_Health()
{
	InitializeHealthBarWidget();
}

void APREnemyCharacter::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeathState();
	}
}
