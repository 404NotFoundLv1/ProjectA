#include "Characters/PRCharacter.h"

#include "Abilities/GA_AssaultBlast.h"
#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_AssaultShield.h"
#include "Abilities/GA_PrimaryAttack.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRDefaultAttributesGameplayEffect.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Camera/CameraComponent.h"
#include "Combat/PRCombatFeedbackComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/PRGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayAbilitySpec.h"
#include "InputAction.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "Deployables/PRDeployableComponent.h"
#include "Roles/PRRoleComponent.h"
#include "ProjectA.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapons/PRWeaponComponent.h"
#include "Items/PRWeaponDataAsset.h"

namespace
{
void ShowInputDebugMessage(const UObject* Context, const TCHAR* ActionName)
{
	UE_LOG(LogProjectA, Log, TEXT("Input action triggered: %s"), ActionName);

	if (GEngine && Context)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Cyan,
			FString::Printf(TEXT("Input: %s"), ActionName));
	}
}

FColor GetDebugColorForPlayerId(const int32 PlayerId)
{
	static const FColor Colors[] = {
		FColor::Cyan,
		FColor::Yellow,
		FColor::Green,
		FColor::Orange,
		FColor::Magenta,
		FColor::Red,
	};

	if (PlayerId < 0)
	{
		return FColor::White;
	}

	return Colors[PlayerId % UE_ARRAY_COUNT(Colors)];
}

const TCHAR* NetRoleToText(const ENetRole Role)
{
	switch (Role)
	{
	case ROLE_Authority:
		return TEXT("Authority");
	case ROLE_AutonomousProxy:
		return TEXT("Autonomous");
	case ROLE_SimulatedProxy:
		return TEXT("Simulated");
	default:
		return TEXT("None");
	}
}
}

APRCharacter::APRCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);
	DefaultAttributesEffectClass = UPRDefaultAttributesGameplayEffect::StaticClass();
	DefaultAbilityClasses.Add(UGA_PrimaryAttack::StaticClass());
	AssaultChargeAbilityClass = UGA_AssaultCharge::StaticClass();
	AssaultBlastAbilityClass = UGA_AssaultBlast::StaticClass();
	AssaultShieldAbilityClass = UGA_AssaultShield::StaticClass();
	AutoRespawnDelay = 5.0f;
	bShowPlayerDebugLabel = false;
	CombatFeedbackComponent = CreateDefaultSubobject<UPRCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));

	PlayerDebugLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlayerDebugLabel"));
	PlayerDebugLabel->SetupAttachment(GetRootComponent());
	PlayerDebugLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	PlayerDebugLabel->SetHorizontalAlignment(EHTA_Center);
	PlayerDebugLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PlayerDebugLabel->SetTextRenderColor(FColor::White);
	PlayerDebugLabel->SetWorldSize(24.0f);
	PlayerDebugLabel->SetText(FText::FromString(TEXT("Player")));
	PlayerDebugLabel->SetHiddenInGame(true);
	PlayerDebugLabel->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpActionAsset.Succeeded())
	{
		JumpAction = JumpActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Move.IA_Move"));
	if (MoveActionAsset.Succeeded())
	{
		MoveAction = MoveActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Look.IA_Look"));
	if (LookActionAsset.Succeeded())
	{
		LookAction = LookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_MouseLook.IA_MouseLook"));
	if (MouseLookActionAsset.Succeeded())
	{
		MouseLookAction = MouseLookActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InteractActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Interact.IA_Interact"));
	if (InteractActionAsset.Succeeded())
	{
		InteractAction = InteractActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> PrimaryAttackActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_PrimaryAttack.IA_PrimaryAttack"));
	if (PrimaryAttackActionAsset.Succeeded())
	{
		PrimaryAttackAction = PrimaryAttackActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Dodge.IA_Dodge"));
	if (DodgeActionAsset.Succeeded())
	{
		DodgeAction = DodgeActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillQActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_Q.IA_Skill_Q"));
	if (SkillQActionAsset.Succeeded())
	{
		SkillQAction = SkillQActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillEActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_E.IA_Skill_E"));
	if (SkillEActionAsset.Succeeded())
	{
		SkillEAction = SkillEActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SkillRActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Skill_R.IA_Skill_R"));
	if (SkillRActionAsset.Succeeded())
	{
		SkillRAction = SkillRActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> OpenInventoryActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_OpenInventory.IA_OpenInventory"));
	if (OpenInventoryActionAsset.Succeeded())
	{
		OpenInventoryAction = OpenInventoryActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AimActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Aim.IA_Aim"));
	if (AimActionAsset.Succeeded())
	{
		AimAction = AimActionAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ReloadActionAsset(TEXT("/Game/ProjectRift/Input/Actions/IA_Reload.IA_Reload"));
	if (ReloadActionAsset.Succeeded())
	{
		ReloadAction = ReloadActionAsset.Object;
	}
}

UAbilitySystemComponent* APRCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

bool APRCharacter::IsCombatUnitInactive() const
{
	return !IsAlive();
}

void APRCharacter::HandleCombatUnitHealthDepleted(const FGameplayEffectContextHandle& EffectContext)
{
	EnterDownedState();
}

void APRCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (const USpringArmComponent* CameraBoomComponent = GetCameraBoom())
	{
		DefaultCameraArmLength = CameraBoomComponent->TargetArmLength;
	}
	if (const UCameraComponent* Camera = GetFollowCamera())
	{
		DefaultCameraFieldOfView = Camera->FieldOfView;
	}
	bDefaultUseControllerRotationYaw = bUseControllerRotationYaw;
	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		bDefaultOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	}
	TargetCameraArmLength = DefaultCameraArmLength;
	TargetCameraFieldOfView = DefaultCameraFieldOfView;

	RefreshPlayerDebugLabel();
}

void APRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAttributeChangeDelegates();
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
		{
			WeaponComponent->HandleAvatarChanged(nullptr);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APRCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocallyControlled())
	{
		return;
	}
	if (USpringArmComponent* CameraBoomComponent = GetCameraBoom())
	{
		CameraBoomComponent->TargetArmLength = FMath::FInterpTo(CameraBoomComponent->TargetArmLength, TargetCameraArmLength, DeltaSeconds, 12.0f);
	}
	if (UCameraComponent* Camera = GetFollowCamera())
	{
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetCameraFieldOfView, DeltaSeconds, 12.0f));
	}
}

void APRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	RefreshPlayerDebugLabel();
	InitializeAbilitySystemFromPlayerState(Cast<APRPlayerState>(GetPlayerState()));
}

void APRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	RefreshPlayerDebugLabel();
	InitializeAbilitySystemFromPlayerState(Cast<APRPlayerState>(GetPlayerState()));
}

bool APRCharacter::InitializeAbilitySystemFromPlayerState(APRPlayerState* InPlayerState)
{
	if (!InPlayerState)
	{
		UE_LOG(LogProjectA, Warning, TEXT("ProjectRift ASC initialization skipped for %s: PlayerState is missing."), *GetNameSafe(this));
		return false;
	}

	UPRAbilitySystemComponent* NewAbilitySystemComponent = InPlayerState->GetProjectRiftAbilitySystemComponent();
	UPRAttributeSet* NewAttributeSet = InPlayerState->GetAttributeSet();
	if (!NewAbilitySystemComponent || !NewAttributeSet)
	{
		UE_LOG(
			LogProjectA,
			Warning,
			TEXT("ProjectRift ASC initialization skipped for %s: ASC=%s AttributeSet=%s."),
			*GetNameSafe(this),
			*GetNameSafe(NewAbilitySystemComponent),
			*GetNameSafe(NewAttributeSet));
		return false;
	}

	const bool bAlreadyInitializedForThisAvatar =
		bAbilitySystemInitialized &&
		AbilitySystemComponent == NewAbilitySystemComponent &&
		AttributeSet == NewAttributeSet &&
		NewAbilitySystemComponent->GetOwnerActor() == InPlayerState &&
		NewAbilitySystemComponent->GetAvatarActor() == this;

	if (bAlreadyInitializedForThisAvatar)
	{
		return true;
	}

	if (AbilitySystemComponent && AbilitySystemComponent != NewAbilitySystemComponent)
	{
		ClearAttributeChangeDelegates();
	}

	AbilitySystemComponent = NewAbilitySystemComponent;
	AttributeSet = NewAttributeSet;
	AbilitySystemComponent->InitAbilityActorInfo(InPlayerState, this);
	if (UPRWeaponComponent* WeaponComponent = InPlayerState->GetWeaponComponent())
	{
		WeaponComponent->HandleAvatarChanged(this);
	}

	if (HasAuthority())
	{
		ApplyDefaultAttributes();
		GrantDefaultAbilities();
		GrantSelectedRoleModuleAbilities();
	}

	BindAttributeChangeDelegates();
	bAbilitySystemInitialized = true;
	OnAbilitySystemInitialized.Broadcast(AbilitySystemComponent);

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("ProjectRift ASC initialized for %s. Owner=%s Avatar=%s LocalRole=%s"),
		*GetNameSafe(this),
		*GetNameSafe(AbilitySystemComponent->GetOwnerActor()),
		*GetNameSafe(AbilitySystemComponent->GetAvatarActor()),
		NetRoleToText(GetLocalRole()));

	return true;
}

bool APRCharacter::GrantDefaultAbilities()
{
	if (bDefaultAbilitiesGranted)
	{
		return true;
	}

	if (!HasAuthority())
	{
		return false;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Default abilities skipped for %s: ASC is missing."), *GetNameSafe(this));
		return false;
	}

	const FGameplayTag PrimaryInputTag = ProjectRiftGameplayTags::Input_Ability_Primary;
	bool bGrantedAnyAbility = false;
	for (const TSubclassOf<UGameplayAbility> AbilityClass : DefaultAbilityClasses)
	{
		if (!AbilityClass)
		{
			continue;
		}

		const FGameplayTag InputTag = AbilityClass == UGA_PrimaryAttack::StaticClass() ? PrimaryInputTag : FGameplayTag();
		bGrantedAnyAbility |= GrantAbilityIfMissing(AbilityClass, InputTag);

		UE_LOG(
			LogProjectA,
			Log,
			TEXT("Granted default ability %s to %s."),
			*GetNameSafe(AbilityClass),
			*GetNameSafe(this));
	}

	bDefaultAbilitiesGranted = bGrantedAnyAbility;
	return bDefaultAbilitiesGranted;
}

bool APRCharacter::GrantSelectedRoleModuleAbilities()
{
	if (bRoleModuleAbilitiesGranted)
	{
		return true;
	}

	if (!HasAuthority())
	{
		return false;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Role module abilities skipped for %s: ASC is missing."), *GetNameSafe(this));
		return false;
	}

	const APRPlayerState* ProjectRiftPlayerState = Cast<APRPlayerState>(GetPlayerState());
	if (!ProjectRiftPlayerState)
	{
		return false;
	}

	UPRRoleComponent* RoleComponent = ProjectRiftPlayerState->GetRoleComponent();
	if (!RoleComponent)
	{
		return false;
	}

	// A profile can be restored before its pawn exists.  Do not replace a
	// catalog-valid, already-unlocked selection during possession/respawn.
	if (!RoleComponent->IsLoadoutValid(ProjectRiftPlayerState->GetSelectedRoleModule(), RoleComponent->GetCurrentLoadout()))
	{
		RoleComponent->EnsureDefaultLoadoutForSelectedRole();
	}
	bRoleModuleAbilitiesGranted = RoleComponent->RefreshGrantedAbilities();
	return bRoleModuleAbilitiesGranted;
}

bool APRCharacter::GrantAbilityIfMissing(const TSubclassOf<UGameplayAbility> AbilityClass, const FGameplayTag InputTag)
{
	if (!HasAuthority() || !AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	if (FGameplayAbilitySpec* ExistingSpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
	{
		if (InputTag.IsValid() && !ExistingSpec->GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			ExistingSpec->GetDynamicSpecSourceTags().AddTag(InputTag);
			AbilitySystemComponent->MarkAbilitySpecDirty(*ExistingSpec);
		}
		return true;
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, 1, INDEX_NONE, this);
	if (InputTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}
	AbilitySystemComponent->GiveAbility(AbilitySpec);

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Granted role module ability %s to %s."),
		*GetNameSafe(AbilityClass),
		*GetNameSafe(this));
	return true;
}

bool APRCharacter::EnterDownedState()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (IsDowned())
	{
		ScheduleAutoRespawn();
		return true;
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	if (DownedStateTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(DownedStateTag, 1, EGameplayTagReplicationState::TagOnly);
	}
	UPRCombatEffectLibrary::ClearNegativeStatusEffects(AbilitySystemComponent);
	AbilitySystemComponent->CancelAllAbilities();
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
		{
			WeaponComponent->SetAiming(false);
			WeaponComponent->CancelReload();
		}
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	ScheduleAutoRespawn();

	UE_LOG(LogProjectA, Log, TEXT("%s entered downed state."), *GetNameSafe(this));
	return true;
}

bool APRCharacter::RespawnFromDowned()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!AbilitySystemComponent || !AttributeSet)
	{
		return false;
	}

	ClearAutoRespawnTimer();

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	const FGameplayTag DeadStateTag = ProjectRiftGameplayTags::State_Dead;
	if (DownedStateTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(DownedStateTag, 1, EGameplayTagReplicationState::TagOnly);
	}
	if (DeadStateTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(DeadStateTag, 1, EGameplayTagReplicationState::TagOnly);
	}

	AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
	AttributeSet->SetShield(AttributeSet->GetMaxShield());

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Walking);
	}
	RefreshMovementFromAttributes();

	UE_LOG(LogProjectA, Log, TEXT("%s respawned from downed state."), *GetNameSafe(this));
	return true;
}

bool APRCharacter::IsDowned() const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayTag DownedStateTag = ProjectRiftGameplayTags::State_Downed;
	return DownedStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(DownedStateTag);
}

bool APRCharacter::IsAlive() const
{
	return AttributeSet && AttributeSet->GetHealth() > 0.0f && !IsDowned();
}

bool APRCharacter::IsAutoRespawnScheduled() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(AutoRespawnTimerHandle);
}

void APRCharacter::ScheduleAutoRespawn()
{
	UWorld* World = GetWorld();
	if (!World || AutoRespawnDelay <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AutoRespawnTimerHandle,
		this,
		&APRCharacter::HandleAutoRespawnTimer,
		AutoRespawnDelay,
		false);
}

void APRCharacter::HandleAutoRespawnTimer()
{
	RespawnFromDowned();
}

void APRCharacter::ClearAutoRespawnTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoRespawnTimerHandle);
	}
}

bool APRCharacter::ApplyDefaultAttributes()
{
	if (bDefaultAttributesApplied)
	{
		return true;
	}

	if (!HasAuthority())
	{
		return false;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Default attributes skipped for %s: ASC is missing."), *GetNameSafe(this));
		return false;
	}

	if (!DefaultAttributesEffectClass)
	{
		UE_LOG(LogProjectA, Warning, TEXT("Default attributes skipped for %s: GE class is missing."), *GetNameSafe(this));
		return false;
	}

	const UGameplayEffect* DefaultAttributesEffect = DefaultAttributesEffectClass->GetDefaultObject<UGameplayEffect>();
	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	AbilitySystemComponent->ApplyGameplayEffectToSelf(DefaultAttributesEffect, 1.0f, EffectContext);
	bDefaultAttributesApplied = true;

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Default attributes applied for %s using %s."),
		*GetNameSafe(this),
		*GetNameSafe(DefaultAttributesEffect));

	return true;
}

void APRCharacter::BindAttributeChangeDelegates()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	ClearAttributeChangeDelegates();

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetHealthAttribute())
		.AddUObject(this, &APRCharacter::HandleHealthChanged);
	ShieldChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetShieldAttribute())
		.AddUObject(this, &APRCharacter::HandleShieldChanged);
	EnergyChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetEnergyAttribute())
		.AddUObject(this, &APRCharacter::HandleEnergyChanged);
	MoveSpeedChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &APRCharacter::HandleMoveSpeedChanged);
	StunnedTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_Stunned, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &APRCharacter::HandleStunnedTagChanged);
	HitStaggeredTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_HitStaggered, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &APRCharacter::HandleHitStaggeredTagChanged);
	RefreshMovementFromAttributes();
}

void APRCharacter::ClearAttributeChangeDelegates()
{
	if (!AbilitySystemComponent)
	{
		HealthChangedDelegateHandle.Reset();
		ShieldChangedDelegateHandle.Reset();
		EnergyChangedDelegateHandle.Reset();
		MoveSpeedChangedDelegateHandle.Reset();
		StunnedTagChangedDelegateHandle.Reset();
		HitStaggeredTagChangedDelegateHandle.Reset();
		return;
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	if (ShieldChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetShieldAttribute())
			.Remove(ShieldChangedDelegateHandle);
		ShieldChangedDelegateHandle.Reset();
	}

	if (EnergyChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetEnergyAttribute())
			.Remove(EnergyChangedDelegateHandle);
		EnergyChangedDelegateHandle.Reset();
	}

	if (MoveSpeedChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UPRAttributeSet::GetMoveSpeedAttribute())
			.Remove(MoveSpeedChangedDelegateHandle);
		MoveSpeedChangedDelegateHandle.Reset();
	}

	if (StunnedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_Stunned, EGameplayTagEventType::NewOrRemoved)
			.Remove(StunnedTagChangedDelegateHandle);
		StunnedTagChangedDelegateHandle.Reset();
	}

	if (HitStaggeredTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(ProjectRiftGameplayTags::State_HitStaggered, EGameplayTagEventType::NewOrRemoved)
			.Remove(HitStaggeredTagChangedDelegateHandle);
		HitStaggeredTagChangedDelegateHandle.Reset();
	}
}

void APRCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(LogProjectA, Log, TEXT("Health changed for %s: %.2f -> %.2f"), *GetNameSafe(this), Data.OldValue, Data.NewValue);
	OnHealthChanged.Broadcast(Data.OldValue, Data.NewValue);

	if (HasAuthority() && Data.NewValue <= 0.0f)
	{
		EnterDownedState();
	}
}

void APRCharacter::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(LogProjectA, Log, TEXT("Shield changed for %s: %.2f -> %.2f"), *GetNameSafe(this), Data.OldValue, Data.NewValue);
	OnShieldChanged.Broadcast(Data.OldValue, Data.NewValue);
}

void APRCharacter::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(LogProjectA, Log, TEXT("Energy changed for %s: %.2f -> %.2f"), *GetNameSafe(this), Data.OldValue, Data.NewValue);
	OnEnergyChanged.Broadcast(Data.OldValue, Data.NewValue);
}

void APRCharacter::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	RefreshMovementFromAttributes();
}

void APRCharacter::HandleStunnedTagChanged(const FGameplayTag StatusTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
		{
			if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
			{
				WeaponComponent->SetAiming(false);
				WeaponComponent->CancelReload();
			}
		}
	}
	RefreshMovementFromAttributes();
}

void APRCharacter::HandleHitStaggeredTagChanged(const FGameplayTag StatusTag, const int32 NewCount)
{
	if (NewCount > 0)
	{
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
		{
			if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
			{
				WeaponComponent->SetAiming(false);
				WeaponComponent->CancelReload();
			}
		}
	}
	else if (CombatFeedbackComponent)
	{
		CombatFeedbackComponent->ClearActiveStagger();
	}
	RefreshMovementFromAttributes();
}

void APRCharacter::RefreshMovementFromAttributes()
{
	if (!AttributeSet || IsDowned())
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

void APRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APRCharacter::DoInteract);
	}

	if (PrimaryAttackAction)
	{
		EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Started, this, &APRCharacter::DoPrimaryAttack);
		EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Completed, this, &APRCharacter::DoPrimaryAttackReleased);
		EnhancedInputComponent->BindAction(PrimaryAttackAction, ETriggerEvent::Canceled, this, &APRCharacter::DoPrimaryAttackReleased);
	}

	if (AimAction)
	{
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &APRCharacter::DoAim);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &APRCharacter::DoAimReleased);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Canceled, this, &APRCharacter::DoAimReleased);
	}

	if (ReloadAction)
	{
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &APRCharacter::DoReload);
	}

	if (DodgeAction)
	{
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &APRCharacter::DoDodge);
	}

	if (SkillQAction)
	{
		EnhancedInputComponent->BindAction(SkillQAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillQ);
		EnhancedInputComponent->BindAction(SkillQAction, ETriggerEvent::Completed, this, &APRCharacter::DoSkillQReleased);
		EnhancedInputComponent->BindAction(SkillQAction, ETriggerEvent::Canceled, this, &APRCharacter::DoSkillQReleased);
	}

	if (SkillEAction)
	{
		EnhancedInputComponent->BindAction(SkillEAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillE);
		EnhancedInputComponent->BindAction(SkillEAction, ETriggerEvent::Completed, this, &APRCharacter::DoSkillEReleased);
		EnhancedInputComponent->BindAction(SkillEAction, ETriggerEvent::Canceled, this, &APRCharacter::DoSkillEReleased);
	}

	if (SkillRAction)
	{
		EnhancedInputComponent->BindAction(SkillRAction, ETriggerEvent::Started, this, &APRCharacter::DoSkillR);
		EnhancedInputComponent->BindAction(SkillRAction, ETriggerEvent::Completed, this, &APRCharacter::DoSkillRReleased);
		EnhancedInputComponent->BindAction(SkillRAction, ETriggerEvent::Canceled, this, &APRCharacter::DoSkillRReleased);
	}

	if (OpenInventoryAction)
	{
		EnhancedInputComponent->BindAction(OpenInventoryAction, ETriggerEvent::Started, this, &APRCharacter::DoOpenInventory);
	}
}

void APRCharacter::RefreshPlayerDebugLabel()
{
	if (!PlayerDebugLabel)
	{
		return;
	}

	PlayerDebugLabel->SetHiddenInGame(!bShowPlayerDebugLabel);
	PlayerDebugLabel->SetVisibility(bShowPlayerDebugLabel);
	if (!bShowPlayerDebugLabel)
	{
		return;
	}

	const APlayerState* CurrentPlayerState = GetPlayerState();
	const int32 PlayerId = CurrentPlayerState ? CurrentPlayerState->GetPlayerId() : INDEX_NONE;
	const FString PlayerName = CurrentPlayerState ? CurrentPlayerState->GetPlayerName() : TEXT("Unassigned");
	const FString LabelText = FString::Printf(
		TEXT("%s\nP%d %s"),
		*PlayerName,
		PlayerId,
		NetRoleToText(GetLocalRole()));

	PlayerDebugLabel->SetText(FText::FromString(LabelText));
	PlayerDebugLabel->SetTextRenderColor(GetDebugColorForPlayerId(PlayerId));
}

void APRCharacter::DoInteract()
{
	ShowInputDebugMessage(this, TEXT("Interact"));

	if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController()))
	{
		ProjectRiftController->TryPickup();
	}
}

void APRCharacter::DoPrimaryAttack()
{
	ShowInputDebugMessage(this, TEXT("PrimaryAttack"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRDeployableComponent* Deployables = ProjectRiftPlayerState->GetDeployableComponent(); Deployables && Deployables->GetPlacementState().bPending)
		{
			if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController()))
			{
				ProjectRiftController->ServerConfirmDeployable(BuildLocalDeployablePreviewTransform(), Deployables->GetPlacementState().SessionSequence);
			}
			return;
		}
	}

	const FGameplayTag PrimaryInputTag = ProjectRiftGameplayTags::Input_Ability_Primary;
	ActivateAbilityInputTag(PrimaryInputTag, TEXT("PrimaryAttack"));
}

void APRCharacter::DoPrimaryAttackReleased()
{
	const FGameplayTag PrimaryInputTag = ProjectRiftGameplayTags::Input_Ability_Primary;
	ReleaseAbilityInputTag(PrimaryInputTag, TEXT("PrimaryAttack"));
}

void APRCharacter::DoAim()
{
	ShowInputDebugMessage(this, TEXT("Aim"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRDeployableComponent* Deployables = ProjectRiftPlayerState->GetDeployableComponent(); Deployables && Deployables->GetPlacementState().bPending)
		{
			if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController()))
			{
				ProjectRiftController->ServerCancelDeployable();
			}
			return;
		}
		if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
		{
			WeaponComponent->SetAiming(true);
		}
	}
}

FTransform APRCharacter::BuildLocalDeployablePreviewTransform() const
{
	const UCameraComponent* Camera = GetFollowCamera();
	const FVector ViewStart = Camera ? Camera->GetComponentLocation() : GetActorLocation();
	const FVector Direction = Camera ? Camera->GetComponentRotation().Vector().GetSafeNormal() : GetActorForwardVector().GetSafeNormal();
	FCollisionObjectQueryParams WorldObjects;
	WorldObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	WorldObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PRDeployablePreview), false, this);
	FHitResult Hit;
	const FVector ViewEnd = ViewStart + Direction * 1600.0f;
	const FVector Location = GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, ViewStart, ViewEnd, WorldObjects, Query)
		? Hit.ImpactPoint : ViewEnd;
	return FTransform(FRotator(0.0f, GetControlRotation().Yaw, 0.0f), Location);
}

void APRCharacter::DoAimReleased()
{
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
		{
			WeaponComponent->SetAiming(false);
		}
	}
}

void APRCharacter::DoReload()
{
	ShowInputDebugMessage(this, TEXT("Reload"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>())
	{
		if (UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState->GetWeaponComponent())
		{
			WeaponComponent->StartReload();
		}
	}
}

void APRCharacter::ApplyWeaponAimPresentation(const bool bNewAiming)
{
	bWeaponAimPresentationActive = bNewAiming;
	const APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>();
	const UPRWeaponComponent* WeaponComponent = ProjectRiftPlayerState ? ProjectRiftPlayerState->GetWeaponComponent() : nullptr;
	const UPRWeaponDataAsset* WeaponData = WeaponComponent ? WeaponComponent->GetEquippedWeaponData() : nullptr;
	TargetCameraArmLength = bNewAiming && WeaponData ? WeaponData->AimArmLength : DefaultCameraArmLength;
	TargetCameraFieldOfView = bNewAiming && WeaponData ? WeaponData->AimFieldOfView : DefaultCameraFieldOfView;
	bUseControllerRotationYaw = bNewAiming ? true : bDefaultUseControllerRotationYaw;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = bNewAiming ? false : bDefaultOrientRotationToMovement;
	}
}

void APRCharacter::DoDodge()
{
	ShowInputDebugMessage(this, TEXT("Dodge"));
}

void APRCharacter::DoSkillQ()
{
	ShowInputDebugMessage(this, TEXT("Skill Q"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>(); ProjectRiftPlayerState && ProjectRiftPlayerState->GetDeployableComponent()->GetPlacementState().bPending)
	{
		if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController())) { ProjectRiftController->ServerCancelDeployable(); }
	}
	const FGameplayTag QInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_Q;
	ActivateAbilityInputTag(QInputTag, TEXT("Skill Q"));
}

void APRCharacter::DoSkillQReleased()
{
	const FGameplayTag QInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_Q;
	ReleaseAbilityInputTag(QInputTag, TEXT("Skill Q"));
}

void APRCharacter::DoSkillE()
{
	ShowInputDebugMessage(this, TEXT("Skill E"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>(); ProjectRiftPlayerState && ProjectRiftPlayerState->GetDeployableComponent()->GetPlacementState().bPending)
	{
		if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController())) { ProjectRiftController->ServerCancelDeployable(); }
	}
	const FGameplayTag EInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_E;
	ActivateAbilityInputTag(EInputTag, TEXT("Skill E"));
}

void APRCharacter::DoSkillEReleased()
{
	const FGameplayTag EInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_E;
	ReleaseAbilityInputTag(EInputTag, TEXT("Skill E"));
}

void APRCharacter::DoSkillR()
{
	ShowInputDebugMessage(this, TEXT("Skill R"));
	if (APRPlayerState* ProjectRiftPlayerState = GetPlayerState<APRPlayerState>(); ProjectRiftPlayerState && ProjectRiftPlayerState->GetDeployableComponent()->GetPlacementState().bPending)
	{
		if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController())) { ProjectRiftController->ServerCancelDeployable(); }
	}
	const FGameplayTag RInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_R;
	ActivateAbilityInputTag(RInputTag, TEXT("Skill R"));
}

void APRCharacter::DoSkillRReleased()
{
	const FGameplayTag RInputTag = ProjectRiftGameplayTags::Input_Ability_Skill_R;
	ReleaseAbilityInputTag(RInputTag, TEXT("Skill R"));
}

void APRCharacter::DoOpenInventory()
{
	ShowInputDebugMessage(this, TEXT("OpenInventory"));

	if (APRPlayerController* ProjectRiftController = Cast<APRPlayerController>(GetController()))
	{
		ProjectRiftController->ToggleInventory();
	}
}

bool APRCharacter::ActivateAbilityInputTag(const FGameplayTag InputTag, const TCHAR* ActionName)
{
	if (IsDowned())
	{
		UE_LOG(LogProjectA, Log, TEXT("%s rejected for %s: character is downed."), ActionName, *GetNameSafe(this));
		return false;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogProjectA, Warning, TEXT("%s skipped for %s: ASC is missing."), ActionName, *GetNameSafe(this));
		return false;
	}

	if (!InputTag.IsValid())
	{
		UE_LOG(LogProjectA, Warning, TEXT("%s skipped for %s: input tag is missing."), ActionName, *GetNameSafe(this));
		return false;
	}

	if (!bRoleModuleAbilitiesGranted && HasAuthority())
	{
		GrantSelectedRoleModuleAbilities();
	}

	const bool bActivated = AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("%s activation for %s: %s"),
		ActionName,
		*GetNameSafe(this),
		bActivated ? TEXT("Activated") : TEXT("Rejected"));
	return bActivated;
}

bool APRCharacter::ReleaseAbilityInputTag(const FGameplayTag InputTag, const TCHAR* ActionName)
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (!InputTag.IsValid())
	{
		return false;
	}

	const bool bReleased = AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("%s release for %s: %s"),
		ActionName,
		*GetNameSafe(this),
		bReleased ? TEXT("Handled") : TEXT("Ignored"));
	return bReleased;
}
