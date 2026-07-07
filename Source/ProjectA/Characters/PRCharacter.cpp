#include "Characters/PRCharacter.h"

#include "Abilities/GA_AssaultBlast.h"
#include "Abilities/GA_AssaultCharge.h"
#include "Abilities/GA_AssaultShield.h"
#include "Abilities/GA_PrimaryAttack.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRDefaultAttributesGameplayEffect.h"
#include "Abilities/PRAttributeSet.h"
#include "Components/TextRenderComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagsManager.h"
#include "InputAction.h"
#include "Player/PRPlayerController.h"
#include "Player/PRPlayerState.h"
#include "ProjectA.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

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
}

void APRCharacter::BeginPlay()
{
	Super::BeginPlay();

	RefreshPlayerDebugLabel();
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

	const FGameplayTag PrimaryInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Primary"), false);
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

	const FName SelectedRoleModule = ProjectRiftPlayerState->GetSelectedRoleModule();
	if (SelectedRoleModule != FName(TEXT("Ability.Role.Assault")) && SelectedRoleModule != FName(TEXT("Role.Assault")))
	{
		return false;
	}

	const FGameplayTag QInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.Q"), false);
	const FGameplayTag EInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.E"), false);
	const FGameplayTag RInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.R"), false);

	bool bGrantedAllAbilities = true;
	bGrantedAllAbilities &= GrantAbilityIfMissing(AssaultChargeAbilityClass, QInputTag);
	bGrantedAllAbilities &= GrantAbilityIfMissing(AssaultBlastAbilityClass, EInputTag);
	bGrantedAllAbilities &= GrantAbilityIfMissing(AssaultShieldAbilityClass, RInputTag);

	bRoleModuleAbilitiesGranted = bGrantedAllAbilities;
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

	const FGameplayTag DownedStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed"), false);
	if (DownedStateTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(DownedStateTag, 1, EGameplayTagReplicationState::TagOnly);
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

	const FGameplayTag DownedStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed"), false);
	const FGameplayTag DeadStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Dead"), false);
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

	UE_LOG(LogProjectA, Log, TEXT("%s respawned from downed state."), *GetNameSafe(this));
	return true;
}

bool APRCharacter::IsDowned() const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayTag DownedStateTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("State.Downed"), false);
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
}

void APRCharacter::ClearAttributeChangeDelegates()
{
	if (!AbilitySystemComponent)
	{
		HealthChangedDelegateHandle.Reset();
		ShieldChangedDelegateHandle.Reset();
		EnergyChangedDelegateHandle.Reset();
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

	const FGameplayTag PrimaryInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Primary"), false);
	ActivateAbilityInputTag(PrimaryInputTag, TEXT("PrimaryAttack"));
}

void APRCharacter::DoPrimaryAttackReleased()
{
	const FGameplayTag PrimaryInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Primary"), false);
	ReleaseAbilityInputTag(PrimaryInputTag, TEXT("PrimaryAttack"));
}

void APRCharacter::DoDodge()
{
	ShowInputDebugMessage(this, TEXT("Dodge"));
}

void APRCharacter::DoSkillQ()
{
	ShowInputDebugMessage(this, TEXT("Skill Q"));
	const FGameplayTag QInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.Q"), false);
	ActivateAbilityInputTag(QInputTag, TEXT("Skill Q"));
}

void APRCharacter::DoSkillQReleased()
{
	const FGameplayTag QInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.Q"), false);
	ReleaseAbilityInputTag(QInputTag, TEXT("Skill Q"));
}

void APRCharacter::DoSkillE()
{
	ShowInputDebugMessage(this, TEXT("Skill E"));
	const FGameplayTag EInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.E"), false);
	ActivateAbilityInputTag(EInputTag, TEXT("Skill E"));
}

void APRCharacter::DoSkillEReleased()
{
	const FGameplayTag EInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.E"), false);
	ReleaseAbilityInputTag(EInputTag, TEXT("Skill E"));
}

void APRCharacter::DoSkillR()
{
	ShowInputDebugMessage(this, TEXT("Skill R"));
	const FGameplayTag RInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.R"), false);
	ActivateAbilityInputTag(RInputTag, TEXT("Skill R"));
}

void APRCharacter::DoSkillRReleased()
{
	const FGameplayTag RInputTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Input.Ability.Skill.R"), false);
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
