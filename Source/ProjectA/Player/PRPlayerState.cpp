#include "Player/PRPlayerState.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Items/PRInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProjectA.h"
#include "Progression/PRMissionProgressionDataAsset.h"
#include "Weapons/PRWeaponComponent.h"

APRPlayerState::APRPlayerState()
	: bIsReady(false)
	, SelectedRoleModule(NAME_None)
	, PlayerDisplayName(TEXT("Player"))
{
	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UPRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UPRAttributeSet>(TEXT("AttributeSet"));
	InventoryComponent = CreateDefaultSubobject<UPRInventoryComponent>(TEXT("InventoryComponent"));
	WeaponComponent = CreateDefaultSubobject<UPRWeaponComponent>(TEXT("WeaponComponent"));
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

void APRPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (APRPlayerState* TargetPlayerState = Cast<APRPlayerState>(PlayerState))
	{
		TargetPlayerState->CopyProjectRiftStateFrom(this);
	}
}

void APRPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (const APRPlayerState* SourcePlayerState = Cast<APRPlayerState>(PlayerState))
	{
		CopyProjectRiftStateFrom(SourcePlayerState);
	}
}

UAbilitySystemComponent* APRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void APRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APRPlayerState, bIsReady);
	DOREPLIFETIME(APRPlayerState, bMultiplayerProfileBound);
	DOREPLIFETIME_CONDITION(APRPlayerState, BoundProfileId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APRPlayerState, BoundStoryProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APRPlayerState, BoundShipModules, COND_OwnerOnly);
	DOREPLIFETIME(APRPlayerState, bRepairPersistencePending);
	DOREPLIFETIME_CONDITION(APRPlayerState, PendingRepairTransactionId, COND_OwnerOnly);
	DOREPLIFETIME(APRPlayerState, SelectedRoleModule);
	DOREPLIFETIME(APRPlayerState, PlayerDisplayName);
	DOREPLIFETIME(APRPlayerState, ShipResources);
}

void APRPlayerState::SetReady(const bool bReady)
{
	if (bReady && (!bMultiplayerProfileBound || bRepairPersistencePending))
	{
		UE_LOG(LogProjectA, Warning, TEXT("Ready state rejected because profile binding or repair persistence is incomplete. Player=%s"), *GetPlayerDisplayName());
		return;
	}
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

bool APRPlayerState::BindMultiplayerProfile(const FPRMultiplayerProfileProjection& Projection, FString& OutDiagnostic)
{
	if (!Projection.IsValid(&OutDiagnostic) || !InventoryComponent || !WeaponComponent)
	{
		return false;
	}
	if (bMultiplayerProfileBound && BoundProfileId == Projection.ProfileId)
	{
		OutDiagnostic = TEXT("Profile is already bound to this PlayerState.");
		return true;
	}

	TArray<FPRShipResourceStack> RuntimeResources;
	RuntimeResources.Reserve(Projection.ResourceWallet.Num());
	for (const FPRProfileResourceBalance& Balance : Projection.ResourceWallet)
	{
		FPRShipResourceStack& RuntimeResource = RuntimeResources.AddDefaulted_GetRef();
		RuntimeResource.ResourceId = Balance.ResourceId;
		RuntimeResource.Count = Balance.Count;
	}
	TArray<FPRItemInstance> PreviousBackpack;
	if (!WeaponComponent->BuildPersistentBackpack(PreviousBackpack, OutDiagnostic))
	{
		return false;
	}
	const TArray<FPRProfileEquipmentEntry> PreviousEquipment = WeaponComponent->GetEquipmentEntries();
	const TArray<FPRShipResourceStack> PreviousResources = ShipResources;
	if (!InventoryComponent->ReplaceInventoryItems(Projection.BackpackItems)
		|| !ReplaceShipResources(RuntimeResources)
		|| !WeaponComponent->ReplaceEquipmentEntries(Projection.Equipment, OutDiagnostic)
		|| !WeaponComponent->EnsureStarterWeapon(TEXT("TestRifle"), OutDiagnostic))
	{
		InventoryComponent->ReplaceInventoryItems(PreviousBackpack);
		ReplaceShipResources(PreviousResources);
		FString RestoreDiagnostic;
		WeaponComponent->ReplaceEquipmentEntries(PreviousEquipment, RestoreDiagnostic);
		OutDiagnostic = TEXT("PlayerState rejected validated multiplayer profile runtime data.");
		return false;
	}

	if (!WeaponComponent->BuildPersistentBackpack(MissionStartBackpackItems, OutDiagnostic))
	{
		InventoryComponent->ReplaceInventoryItems(PreviousBackpack);
		ReplaceShipResources(PreviousResources);
		FString RestoreDiagnostic;
		WeaponComponent->ReplaceEquipmentEntries(PreviousEquipment, RestoreDiagnostic);
		return false;
	}
	BoundProfileId = Projection.ProfileId;
	bMultiplayerProfileBound = true;
	BoundStoryProgress = Projection.Story;
	BoundShipModules = Projection.ShipModules;
	bRepairPersistencePending = false;
	PendingRepairTransactionId.Invalidate();
	MissionStartResourceWallet = Projection.ResourceWallet;
	MissionStartRoleId = Projection.SelectedRoleId;
	SetPlayerDisplayName(Projection.DisplayName);
	SetSelectedRoleModule(Projection.SelectedRoleId);
	ForceNetUpdate();
	return true;
}

void APRPlayerState::ClearMultiplayerProfileBinding()
{
	bIsReady = false;
	bMultiplayerProfileBound = false;
	BoundProfileId.Invalidate();
	BoundStoryProgress = FPRProfileStoryProgress();
	BoundShipModules.Reset();
	bRepairPersistencePending = false;
	PendingRepairTransactionId.Invalidate();
	MissionStartBackpackItems.Reset();
	MissionStartResourceWallet.Reset();
	MissionStartRoleId = NAME_None;
	ForceNetUpdate();
}

bool APRPlayerState::ApplyMissionCompletion(const UPRMissionProgressionDataAsset* Mission)
{
	return Mission && Mission->ApplyCompletion(BoundStoryProgress);
}

bool APRPlayerState::BuildBoundShipRepairSnapshot(FPRProfileSnapshot& OutSnapshot, FString& OutDiagnostic) const
{
	OutSnapshot = FPRProfileSnapshot();
	if (!bMultiplayerProfileBound || !BoundProfileId.IsValid())
	{
		OutDiagnostic = TEXT("A valid multiplayer profile must be bound before evaluating ship repairs.");
		return false;
	}
	OutSnapshot.ResourceWallet.Reserve(ShipResources.Num());
	for (const FPRShipResourceStack& Resource : ShipResources)
	{
		if (!Resource.IsValid())
		{
			OutDiagnostic = TEXT("Bound runtime resources contain invalid data.");
			return false;
		}
		OutSnapshot.ResourceWallet.Emplace(Resource.ResourceId, Resource.Count);
	}
	if (!WeaponComponent || !WeaponComponent->BuildPersistentBackpack(OutSnapshot.BackpackItems, OutDiagnostic))
	{
		return false;
	}
	OutSnapshot.Equipment = WeaponComponent->GetEquipmentEntries();
	OutSnapshot.ShipModules = BoundShipModules;
	OutSnapshot.Story = BoundStoryProgress;
	OutSnapshot.SelectedRoleId = SelectedRoleModule;
	if (!SelectedRoleModule.IsNone())
	{
		OutSnapshot.UnlockedRoleIds.Add(SelectedRoleModule);
	}
	OutSnapshot.Normalize();
	return OutSnapshot.IsValid(&OutDiagnostic);
}

bool APRPlayerState::ApplyShipRepairState(const FPRShipRepairReceipt& Receipt, FString& OutDiagnostic)
{
	if (!bMultiplayerProfileBound || Receipt.ProfileId != BoundProfileId || !Receipt.IsValid(&OutDiagnostic))
	{
		if (OutDiagnostic.IsEmpty())
		{
			OutDiagnostic = TEXT("Ship repair receipt does not match the bound profile.");
		}
		return false;
	}
	TArray<FPRShipResourceStack> RuntimeResources;
	RuntimeResources.Reserve(Receipt.SettledResourceWallet.Num());
	for (const FPRProfileResourceBalance& Balance : Receipt.SettledResourceWallet)
	{
		FPRShipResourceStack& RuntimeResource = RuntimeResources.AddDefaulted_GetRef();
		RuntimeResource.ResourceId = Balance.ResourceId;
		RuntimeResource.Count = Balance.Count;
	}
	if (!ReplaceShipResources(RuntimeResources))
	{
		OutDiagnostic = TEXT("PlayerState rejected the repaired resource wallet.");
		return false;
	}
	BoundShipModules = Receipt.SettledShipModules;
	BoundStoryProgress = Receipt.SettledStory;
	MissionStartResourceWallet = Receipt.SettledResourceWallet;
	bIsReady = false;
	ForceNetUpdate();
	return true;
}

void APRPlayerState::SetRepairPersistencePending(const FGuid TransactionId, const bool bPending)
{
	if (bPending)
	{
		if (!bMultiplayerProfileBound || !TransactionId.IsValid())
		{
			return;
		}
		bRepairPersistencePending = true;
		PendingRepairTransactionId = TransactionId;
		bIsReady = false;
	}
	else if (!TransactionId.IsValid() || PendingRepairTransactionId == TransactionId)
	{
		bRepairPersistencePending = false;
		PendingRepairTransactionId.Invalidate();
	}
	ForceNetUpdate();
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

int32 APRPlayerState::GetShipResourceCount(const FName ResourceId) const
{
	const int32 ResourceIndex = FindShipResourceIndex(ResourceId);
	return ResourceIndex == INDEX_NONE ? 0 : FMath::Max(0, ShipResources[ResourceIndex].Count);
}

bool APRPlayerState::AddShipResource(const FName ResourceId, const int32 Count)
{
	if (ResourceId.IsNone() || Count <= 0)
	{
		return false;
	}

	const int32 ResourceIndex = FindShipResourceIndex(ResourceId);
	if (ResourceIndex == INDEX_NONE)
	{
		FPRShipResourceStack& NewResource = ShipResources.AddDefaulted_GetRef();
		NewResource.ResourceId = ResourceId;
		NewResource.Count = Count;
	}
	else
	{
		ShipResources[ResourceIndex].Count = FMath::Max(0, ShipResources[ResourceIndex].Count) + Count;
	}

	ForceNetUpdate();

	UE_LOG(
		LogProjectA,
		Log,
		TEXT("Ship resource added. Player=%s ResourceId=%s Added=%d Total=%d"),
		*GetPlayerDisplayName(),
		*ResourceId.ToString(),
		Count,
		GetShipResourceCount(ResourceId));

	return true;
}

void APRPlayerState::ResetShipResources()
{
	if (ShipResources.IsEmpty())
	{
		return;
	}

	ShipResources.Reset();
	ForceNetUpdate();
}

bool APRPlayerState::ReplaceShipResources(const TArray<FPRShipResourceStack>& Resources)
{
	TSet<FName> ResourceIds;
	for (const FPRShipResourceStack& Resource : Resources)
	{
		if (!Resource.IsValid() || ResourceIds.Contains(Resource.ResourceId))
		{
			return false;
		}
		ResourceIds.Add(Resource.ResourceId);
	}
	ShipResources = Resources;
	ForceNetUpdate();
	return true;
}

FString APRPlayerState::BuildShipResourceSummary() const
{
	TArray<FPRShipResourceStack> ValidResources;
	ValidResources.Reserve(ShipResources.Num());
	for (const FPRShipResourceStack& Resource : ShipResources)
	{
		if (Resource.IsValid())
		{
			ValidResources.Add(Resource);
		}
	}

	if (ValidResources.IsEmpty())
	{
		return TEXT("None");
	}

	ValidResources.Sort(
		[](const FPRShipResourceStack& Left, const FPRShipResourceStack& Right)
		{
			return Left.ResourceId.ToString() < Right.ResourceId.ToString();
		});

	TArray<FString> ResourceParts;
	ResourceParts.Reserve(ValidResources.Num());
	for (const FPRShipResourceStack& Resource : ValidResources)
	{
		ResourceParts.Add(FString::Printf(TEXT("%s x%d"), *Resource.ResourceId.ToString(), Resource.Count));
	}

	return FString::Join(ResourceParts, TEXT(", "));
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

void APRPlayerState::OnRep_ShipResources()
{
}

int32 APRPlayerState::FindShipResourceIndex(const FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return INDEX_NONE;
	}

	return ShipResources.IndexOfByPredicate(
		[ResourceId](const FPRShipResourceStack& Resource)
		{
			return Resource.ResourceId == ResourceId;
		});
}

void APRPlayerState::CopyProjectRiftStateFrom(const APRPlayerState* SourcePlayerState)
{
	if (!SourcePlayerState)
	{
		return;
	}

	bIsReady = false;
	bMultiplayerProfileBound = SourcePlayerState->bMultiplayerProfileBound;
	BoundProfileId = SourcePlayerState->BoundProfileId;
	BoundStoryProgress = SourcePlayerState->BoundStoryProgress;
	BoundShipModules = SourcePlayerState->BoundShipModules;
	bRepairPersistencePending = SourcePlayerState->bRepairPersistencePending;
	PendingRepairTransactionId = SourcePlayerState->PendingRepairTransactionId;
	MissionStartBackpackItems = SourcePlayerState->MissionStartBackpackItems;
	MissionStartResourceWallet = SourcePlayerState->MissionStartResourceWallet;
	MissionStartRoleId = SourcePlayerState->MissionStartRoleId;
	if (InventoryComponent && SourcePlayerState->InventoryComponent)
	{
		if (!InventoryComponent->ReplaceInventoryItems(SourcePlayerState->InventoryComponent->GetInventoryItems()))
		{
			UE_LOG(LogProjectA, Error, TEXT("Inventory state failed to copy during seamless PlayerState replacement. Source=%s Target=%s"),
				*GetNameSafe(SourcePlayerState), *GetNameSafe(this));
		}
	}
	if (WeaponComponent && SourcePlayerState->WeaponComponent)
	{
		WeaponComponent->CopyRuntimeStateFrom(SourcePlayerState->WeaponComponent);
	}
	SelectedRoleModule = SourcePlayerState->SelectedRoleModule;
	PlayerDisplayName = SourcePlayerState->PlayerDisplayName;
	ShipResources = SourcePlayerState->ShipResources;
	ForceNetUpdate();
}
