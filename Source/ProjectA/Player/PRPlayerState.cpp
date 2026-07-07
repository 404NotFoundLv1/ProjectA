#include "Player/PRPlayerState.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Items/PRInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"

APRPlayerState::APRPlayerState()
	: bIsReady(false)
	, SelectedRoleModule(NAME_None)
	, PlayerDisplayName(TEXT("Player"))
{
	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UPRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UPRAttributeSet>(TEXT("AttributeSet"));
	InventoryComponent = CreateDefaultSubobject<UPRInventoryComponent>(TEXT("InventoryComponent"));
}

void APRPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("ProjectRift GAS initialized for %s. ASC=%s AttributeSet=%s"),
		*GetNameSafe(this),
		*GetNameSafe(AbilitySystemComponent),
		*GetNameSafe(AttributeSet));
}

UAbilitySystemComponent* APRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void APRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRPlayerState, bIsReady);
	DOREPLIFETIME(APRPlayerState, SelectedRoleModule);
	DOREPLIFETIME(APRPlayerState, PlayerDisplayName);
}

void APRPlayerState::SetReady(const bool bReady)
{
	if (bIsReady == bReady)
	{
		return;
	}

	bIsReady = bReady;
	ForceNetUpdate();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Lobby ready state changed: %s is %s"),
		*GetPlayerDisplayName(),
		bIsReady ? TEXT("Ready") : TEXT("Not Ready"));
}

void APRPlayerState::SetSelectedRoleModule(const FName InSelectedRoleModule)
{
	if (SelectedRoleModule == InSelectedRoleModule)
	{
		return;
	}

	SelectedRoleModule = InSelectedRoleModule;
	ForceNetUpdate();
}

void APRPlayerState::SetPlayerDisplayName(const FString& InPlayerDisplayName)
{
	const FString SanitizedDisplayName = InPlayerDisplayName.TrimStartAndEnd();
	const FString NewDisplayName = SanitizedDisplayName.IsEmpty() ? TEXT("Player") : SanitizedDisplayName;

	if (PlayerDisplayName == NewDisplayName)
	{
		return;
	}

	PlayerDisplayName = NewDisplayName;
	ForceNetUpdate();
}

void APRPlayerState::OnRep_IsReady()
{
	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Lobby ready state replicated: %s is %s"),
		*GetPlayerDisplayName(),
		bIsReady ? TEXT("Ready") : TEXT("Not Ready"));
}

void APRPlayerState::OnRep_PlayerDisplayName()
{
	if (PlayerDisplayName.IsEmpty())
	{
		PlayerDisplayName = TEXT("Player");
	}
}
