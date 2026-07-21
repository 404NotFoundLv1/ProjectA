#include "Enemies/PREnemyCharacter.h"

#include "Abilities/GA_EnemyMeleeAttack.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Abilities/PRDamageGameplayEffect.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Characters/PRCharacter.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRAssetManager.h"
#include "Core/PRRiftGameMode.h"
#include "Core/PRRiftRuleComponent.h"
#include "Enemies/PREnemyAIController.h"
#include "Enemies/PREnemyBehaviorComponent.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "Enemies/PREnemyThreatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
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

	AbilitySystemComponent = CreateDefaultSubobject<UPRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UPRAttributeSet>(TEXT("AttributeSet"));
	CombatFeedbackComponent = CreateDefaultSubobject<UPRCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));
	EnemyThreatComponent = CreateDefaultSubobject<UPREnemyThreatComponent>(TEXT("EnemyThreatComponent"));
	EnemyBehaviorComponent = CreateDefaultSubobject<UPREnemyBehaviorComponent>(TEXT("EnemyBehaviorComponent"));
	EnemyMeleeAbilityClass = UGA_EnemyMeleeAttack::StaticClass();

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

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		if (HasAuthority())
		{
			const FPREnemyAttributeDefinition* DefinitionAttributes = EnemyDefinition ? &EnemyDefinition->Attributes : nullptr;
			const float InitialMaxHealth = FMath::Max((DefinitionAttributes ? DefinitionAttributes->MaxHealth : MaxHealth) * SpawnHealthMultiplier, 1.0f);
			AttributeSet->SetMaxHealth(InitialMaxHealth);
			AttributeSet->SetHealth(InitialMaxHealth);
			const float MaxShield = DefinitionAttributes ? DefinitionAttributes->MaxShield : 0.0f;
			const float InitialShield = DefinitionAttributes ? DefinitionAttributes->InitialShield : 0.0f;
			AttributeSet->SetMaxShield(MaxShield);
			AttributeSet->SetShield(FMath::Min(InitialShield, MaxShield));
			AttributeSet->SetMaxEnergy(0.0f);
			AttributeSet->SetEnergy(0.0f);
			AttributeSet->SetAttackPower((DefinitionAttributes ? DefinitionAttributes->AttackPower : 10.0f) * SpawnAttackPowerMultiplier);
			AttributeSet->SetMoveSpeed(DefinitionAttributes ? DefinitionAttributes->MoveSpeed : 360.0f);
			AttributeSet->SetCooldownReduction(0.0f);
			AttributeSet->SetHealingPower(0.0f);
			AttributeSet->SetPollutionResistance(0.0f);

			if (EnemyMeleeAbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(EnemyMeleeAbilityClass))
			{
				AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(EnemyMeleeAbilityClass, 1));
			}
		}

		BindCombatDelegates();
		RefreshMovementFromAttributes();
	}

	InitializeHealthBarWidget();
	if (!HasAuthority())
	{
		ResolveEnemyDefinitionFromId();
	}
	if (HasAuthority() && EnemyBehaviorComponent && EnemyDefinition)
	{
		EnemyBehaviorComponent->StartBehavior();
	}
}

void APREnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APREnemyCharacter, HuntTargetId);
	DOREPLIFETIME(APREnemyCharacter, EnemyDefinitionId);
}

UAbilitySystemComponent* APREnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

bool APREnemyCharacter::IsCombatUnitInactive() const
{
	return IsDead();
}

void APREnemyCharacter::HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext)
{
	AController* DeathInstigator = Cast<AController>(EffectContext.GetOriginalInstigator());
	if (!DeathInstigator)
	{
		if (const APawn* InstigatorPawn = Cast<APawn>(EffectContext.GetOriginalInstigator()))
		{
			DeathInstigator = InstigatorPawn->GetController();
		}
	}
	HandleDeath(DeathInstigator);
}

void APREnemyCharacter::HandleCombatUnitDamageResolved(AActor* DamageSource, const float ResolvedDamage)
{
	if (!HasAuthority() || !EnemyThreatComponent || ResolvedDamage <= 0.0f)
	{
		return;
	}
	APRCharacter* PlayerSource = Cast<APRCharacter>(DamageSource);
	if (!PlayerSource)
	{
		if (APawn* SourcePawn = Cast<APawn>(DamageSource))
		{
			PlayerSource = Cast<APRCharacter>(SourcePawn);
		}
	}
	EnemyThreatComponent->AddThreat(PlayerSource, ResolvedDamage);
}

float APREnemyCharacter::TakeDamage(
	const float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	return ApplyEnemyDamage(DamageAmount, EventInstigator) ? DamageAmount : 0.0f;
}

bool APREnemyCharacter::IsDead() const
{
	return bDeathHandled
		|| (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead));
}

float APREnemyCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float APREnemyCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : FMath::Max(MaxHealth, 1.0f);
}

float APREnemyCharacter::GetHealthPercent() const
{
	const float RuntimeMaxHealth = GetMaxHealth();
	return RuntimeMaxHealth > 0.0f ? FMath::Clamp(GetHealth() / RuntimeMaxHealth, 0.0f, 1.0f) : 0.0f;
}

float APREnemyCharacter::GetAttackDamage() const
{
	if (EnemyDefinition)
	{
		for (const FPREnemyActionDefinition& Action : EnemyDefinition->Actions)
		{
			if (Action.Kind == EPREnemyActionKind::Melee && Action.BaseDamage > 0.0f)
			{
				return Action.BaseDamage;
			}
		}
	}
	const UGA_EnemyMeleeAttack* MeleeAbility = EnemyMeleeAbilityClass
		? EnemyMeleeAbilityClass->GetDefaultObject<UGA_EnemyMeleeAttack>()
		: nullptr;
	return MeleeAbility ? MeleeAbility->GetBaseDamage() : AttackDamage;
}

void APREnemyCharacter::SetSpawnHealthMultiplier(const float InMultiplier)
{
	if (HasAuthority())
	{
		SpawnHealthMultiplier = FMath::IsFinite(InMultiplier) ? FMath::Clamp(InMultiplier, 0.1f, 20.0f) : 1.0f;
	}
}

void APREnemyCharacter::SetSpawnAttackPowerMultiplier(const float InMultiplier)
{
	if (HasAuthority())
	{
		SpawnAttackPowerMultiplier = FMath::IsFinite(InMultiplier) ? FMath::Clamp(InMultiplier, 0.1f, 20.0f) : 1.0f;
	}
}

bool APREnemyCharacter::SetEnemyDefinition(UPREnemyDefinitionDataAsset* InDefinition)
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

	EnemyDefinition = InDefinition;
	EnemyDefinitionId = InDefinition->EnemyId;
	return !EnemyDefinitionId.IsNone();
}

void APREnemyCharacter::OnRep_EnemyDefinitionId()
{
	ResolveEnemyDefinitionFromId();
}

void APREnemyCharacter::ResolveEnemyDefinitionFromId()
{
	if (!EnemyDefinition && !EnemyDefinitionId.IsNone())
	{
		if (UPRAssetManager* AssetManager = UPRAssetManager::Get())
		{
			EnemyDefinition = AssetManager->LoadEnemyDefinitionSync(EnemyDefinitionId);
		}
	}
}

bool APREnemyCharacter::IsStatusImmune(const FGameplayTag StatusTag) const
{
	return StatusTag.IsValid() && EnemyDefinition && EnemyDefinition->ImmunityTags.HasTagExact(StatusTag);
}

bool APREnemyCharacter::IsHeavyHitReactionsOnly() const
{
	return EnemyDefinition && EnemyDefinition->bHeavyHitReactionsOnly;
}

FString APREnemyCharacter::GetActiveStatusText() const
{
	FString StatusText = UPRCombatEffectLibrary::GetActiveNegativeStatusText(AbilitySystemComponent);
	if (CombatFeedbackComponent && CombatFeedbackComponent->LastHandledCueTag.IsValid())
	{
		const FString CueDebugText = FString::Printf(
			TEXT("CUE=%s A:%d H:%d"),
			*CombatFeedbackComponent->LastHandledCueTag.ToString(),
			CombatFeedbackComponent->ActiveStatusCueTags.Num(),
			CombatFeedbackComponent->GameplayCueHandledCount);
		StatusText = StatusText == TEXT("None")
			? CueDebugText
			: FString::Printf(TEXT("%s | %s"), *StatusText, *CueDebugText);
	}
	return StatusText;
}

bool APREnemyCharacter::ApplyEnemyDamage(const float DamageAmount, AController* DamageInstigator)
{
	if (!HasAuthority() || IsDead() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f || !AbilitySystemComponent)
	{
		return false;
	}

	UAbilitySystemComponent* SourceAbilitySystem = nullptr;
	UObject* SourceObject = nullptr;
	if (DamageInstigator)
	{
		SourceObject = DamageInstigator->GetPawn();
		SourceAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Cast<AActor>(SourceObject));
		if (!SourceAbilitySystem)
		{
			SourceObject = DamageInstigator->PlayerState.Get();
			SourceAbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Cast<AActor>(SourceObject));
		}
	}

	if (SourceAbilitySystem)
	{
		return UPRCombatEffectLibrary::ApplyDamageToTarget(
			SourceAbilitySystem,
			AbilitySystemComponent,
			DamageAmount,
			ProjectRiftGameplayTags::Damage_Type_Physical,
			SourceObject);
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		UPRDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Damage, DamageAmount);
	SpecHandle.Data->AddDynamicAssetTag(ProjectRiftGameplayTags::Damage_Type_Physical);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}

bool APREnemyCharacter::TryMeleeAttack(AActor* TargetActor)
{
	if (!HasAuthority() || IsDead() || !TargetActor || !AbilitySystemComponent)
	{
		return false;
	}

	const UGA_EnemyMeleeAttack* MeleeAbility = EnemyMeleeAbilityClass
		? EnemyMeleeAbilityClass->GetDefaultObject<UGA_EnemyMeleeAttack>()
		: nullptr;
	if (!MeleeAbility || !MeleeAbility->IsTargetInRange(this, TargetActor))
	{
		return false;
	}

	FGameplayEventData EventData;
	EventData.EventTag = ProjectRiftGameplayTags::Event_Ability_Enemy_Melee;
	EventData.Instigator = this;
	EventData.Target = TargetActor;
	return AbilitySystemComponent->HandleGameplayEvent(
		ProjectRiftGameplayTags::Event_Ability_Enemy_Melee,
		&EventData) > 0;
}

APRPickupActor* APREnemyCharacter::SpawnDeathLoot()
{
	if (!HasAuthority() || bDeathLootSpawned)
	{
		return nullptr;
	}

	UPRLootTableDataAsset* LootTableToUse = EnemyDefinition && EnemyDefinition->LootTable
		? EnemyDefinition->LootTable.Get() : DeathLootTable.Get();
	if (EnemyDefinition && !EnemyDefinition->bDropsLoot)
	{
		return nullptr;
	}
	if (!LootTableToUse)
	{
		LootTableToUse = NewObject<UPRLootTableDataAsset>(this);
	}

	const FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, 28.0f);
	FPRRewardSourceContext RewardSource;
	RewardSource.SourceType = EPRRewardSourceType::Enemy;
	RewardSource.SourceId = EnemyDefinition && !EnemyDefinition->EnemyId.IsNone()
		? EnemyDefinition->EnemyId : FName(TEXT("Enemy.Basic"));
	if (const APRRiftGameMode* RiftGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		RewardSource.Seed = RiftGameMode->AllocateRewardSeed(RewardSource.SourceType, RewardSource.SourceId, FGuid(), 0);
	}
	APRPickupActor* LootPickup = nullptr;
	float MaterialCountMultiplier = 1.0f;
	if (const APRRiftGameMode* RiftGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APRRiftGameMode>() : nullptr)
	{
		if (const UPRRiftRuleComponent* Rules = RiftGameMode->GetRiftRuleComponent())
		{
			MaterialCountMultiplier = Rules->GetWorldMaterialLootMultiplier();
		}
	}
	if (DeathLootRollOverride >= 0.0f)
	{
		// Retained solely for deterministic legacy test setups.
		LootPickup = UPRLootTableLibrary::SpawnLootPickupFromTable(
			this,
			LootTableToUse,
			PickupActorClass,
			SpawnLocation,
			FRotator::ZeroRotator,
			DeathLootRollOverride,
			MaterialCountMultiplier);
	}
	else
	{
		// The authoritative server owns the seed; generated equipment persists the final result.
		LootPickup = UPRLootTableLibrary::SpawnSeededLootPickupFromTable(
			this,
			LootTableToUse,
			PickupActorClass,
			SpawnLocation,
			FRotator::ZeroRotator,
			RewardSource.Seed != 0 ? RewardSource.Seed : FMath::Rand(),
			MaterialCountMultiplier);
	}
	if (LootPickup)
	{
		LootPickup->SetRewardSource(RewardSource);
	}

	bDeathLootSpawned = LootPickup != nullptr;
	return LootPickup;
}

void APREnemyCharacter::HandleDeath(AController* DeathInstigator)
{
	if (bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	if (AttributeSet)
	{
		AttributeSet->SetHealth(0.0f);
	}
	if (AbilitySystemComponent)
	{
		UPRCombatEffectLibrary::ClearNegativeStatusEffects(AbilitySystemComponent);
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(
			ProjectRiftGameplayTags::State_Dead,
			1,
			EGameplayTagReplicationState::TagOnly);
	}

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

void APREnemyCharacter::BindCombatDelegates()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APREnemyCharacter::HandleHealthChanged);
	MoveSpeedChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &APREnemyCharacter::HandleMoveSpeedChanged);
	AbilitySystemComponent
		->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_Stunned, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &APREnemyCharacter::HandleStunnedTagChanged);
	HitStaggeredTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_HitStaggered, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &APREnemyCharacter::HandleHitStaggeredTagChanged);
	DeadTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &APREnemyCharacter::HandleDeadTagChanged);
}

void APREnemyCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	InitializeHealthBarWidget();
}

void APREnemyCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	RefreshMovementFromAttributes();
}

void APREnemyCharacter::HandleStunnedTagChanged(const FGameplayTag StatusTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}
	RefreshMovementFromAttributes();
}

void APREnemyCharacter::HandleHitStaggeredTagChanged(const FGameplayTag StatusTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
	}
	else if (CombatFeedbackComponent)
	{
		CombatFeedbackComponent->ClearActiveStagger();
	}
	RefreshMovementFromAttributes();
}

void APREnemyCharacter::HandleDeadTagChanged(const FGameplayTag StatusTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplyDeathState();
	}
}

void APREnemyCharacter::RefreshMovementFromAttributes()
{
	if (!AttributeSet || IsDead())
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		const bool bMovementBlocked = AbilitySystemComponent
			&& (AbilitySystemComponent->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned)
				|| AbilitySystemComponent->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered));
		MovementComponent->MaxWalkSpeed = bMovementBlocked ? 0.0f : FMath::Max(AttributeSet->GetMoveSpeed(), 0.0f);
	}
}
