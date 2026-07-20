#include "Items/PRQuickbarComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRCombatEffectLibrary.h"
#include "Characters/PRCharacter.h"
#include "GameplayEffect.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRItemDataAsset.h"
#include "Items/PRItemTransactionComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "TimerManager.h"
#include "Weapons/PRWeaponComponent.h"

namespace
{
// Resolve gameplay tags only after the gameplay-tags manager is initialized.
// File-scope dynamic tag construction can run before that point in a packaged game.
const FGameplayTag& UsingItemTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.UsingItem"), false); return Tag; }
const FGameplayTag& DownedTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Downed"), false); return Tag; }
const FGameplayTag& DeadTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"), false); return Tag; }
const FGameplayTag& StunnedTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"), false); return Tag; }
const FGameplayTag& HitStaggeredTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.HitStaggered"), false); return Tag; }
const FGameplayTag& PollutedTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Polluted"), false); return Tag; }
const FGameplayTag& SlowedTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Slowed"), false); return Tag; }
}

UPRQuickbarComponent::UPRQuickbarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPRQuickbarComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPRInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UPRQuickbarComponent::HandleInventoryChanged);
	}
	ReconcileInventoryReferences();
}

void UPRQuickbarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPRInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UPRQuickbarComponent::HandleInventoryChanged);
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CancelActiveUseAuthoritative(TEXT("Quickbar owner ended play."));
	}
	Super::EndPlay(EndPlayReason);
}

void UPRQuickbarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPRQuickbarComponent, QuickSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPRQuickbarComponent, ActiveUse, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPRQuickbarComponent, Cooldowns, COND_OwnerOnly);
}

bool UPRQuickbarComponent::AssignSlot(const int32 SlotIndex, const FGuid InstanceGuid)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerAssignSlot(SlotIndex, InstanceGuid);
		return false;
	}
	if (!IsSlotIndexValid(SlotIndex) || !InstanceGuid.IsValid())
	{
		return false;
	}
	UPRItemDataAsset* Definition = FindItemDefinition(InstanceGuid);
	if (!Definition || !Definition->UseDefinition.IsUsableFromQuickbar())
	{
		return false;
	}
	QuickSlots.RemoveAll([SlotIndex](const FPRQuickSlotReference& Slot) { return Slot.SlotIndex == SlotIndex; });
	QuickSlots.RemoveAll([InstanceGuid](const FPRQuickSlotReference& Slot) { return Slot.InstanceGuid == InstanceGuid; });
	QuickSlots.Emplace(FPRQuickSlotReference{ SlotIndex, InstanceGuid });
	QuickSlots.Sort([](const FPRQuickSlotReference& Left, const FPRQuickSlotReference& Right) { return Left.SlotIndex < Right.SlotIndex; });
	BroadcastChanged();
	return true;
}

bool UPRQuickbarComponent::ClearSlot(const int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerClearSlot(SlotIndex);
		return false;
	}
	if (!IsSlotIndexValid(SlotIndex))
	{
		return false;
	}
	const int32 Removed = QuickSlots.RemoveAll([SlotIndex](const FPRQuickSlotReference& Slot) { return Slot.SlotIndex == SlotIndex; });
	if (Removed > 0)
	{
		BroadcastChanged();
	}
	return Removed > 0;
}

bool UPRQuickbarComponent::RequestUseSlot(const int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerRequestUseSlot(SlotIndex);
		return false;
	}
	const FPRQuickSlotReference* Slot = FindSlot(SlotIndex);
	return Slot ? BeginUseAuthoritative(*Slot) : false;
}

bool UPRQuickbarComponent::RequestUseInstance(const FGuid InstanceGuid)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerRequestUseInstance(InstanceGuid);
		return false;
	}
	if (!InstanceGuid.IsValid())
	{
		return false;
	}
	FPRQuickSlotReference SourceReference;
	SourceReference.InstanceGuid = InstanceGuid;
	return BeginUseAuthoritative(SourceReference);
}

void UPRQuickbarComponent::CancelActiveUse()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerCancelActiveUse();
		return;
	}
	CancelActiveUseAuthoritative(TEXT("Cancelled by owner request."));
}

void UPRQuickbarComponent::CopyRuntimeStateFrom(const UPRQuickbarComponent* SourceComponent)
{
	if (!SourceComponent)
	{
		return;
	}
	QuickSlots = SourceComponent->QuickSlots;
	Cooldowns = SourceComponent->Cooldowns;
	ActiveUse = FPRQuickbarUseState();
	ReconcileInventoryReferences();
}

void UPRQuickbarComponent::ResetForNewProfileBinding()
{
	CancelActiveUseAuthoritative(TEXT("Profile binding replaced quickbar state."));
	QuickSlots.Reset();
	Cooldowns.Reset();
	BroadcastChanged();
}

void UPRQuickbarComponent::SetQuickSlotsFromProfile(const TArray<FPRQuickSlotReference>& InQuickSlots)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	QuickSlots = InQuickSlots;
	ReconcileInventoryReferences();
}

void UPRQuickbarComponent::ReconcileInventoryReferences()
{
	UPRInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}
	const int32 Removed = QuickSlots.RemoveAll([Inventory](const FPRQuickSlotReference& Slot)
	{
		return !Slot.IsValid() || !Inventory->GetItems().ContainsByPredicate([&Slot](const FPRItemInstance& Item)
		{
			return Item.InstanceGuid == Slot.InstanceGuid;
		});
	});
	if (ActiveUse.IsActive() && !Inventory->GetItems().ContainsByPredicate([this](const FPRItemInstance& Item)
	{
		return Item.InstanceGuid == ActiveUse.InstanceGuid;
	}))
	{
		CancelActiveUseAuthoritative(TEXT("The active quickbar source item was removed."));
	}
	if (Removed > 0)
	{
		BroadcastChanged();
	}
}

void UPRQuickbarComponent::ServerAssignSlot_Implementation(const int32 SlotIndex, const FGuid InstanceGuid)
{
	AssignSlot(SlotIndex, InstanceGuid);
}

void UPRQuickbarComponent::ServerClearSlot_Implementation(const int32 SlotIndex)
{
	ClearSlot(SlotIndex);
}

void UPRQuickbarComponent::ServerRequestUseSlot_Implementation(const int32 SlotIndex)
{
	RequestUseSlot(SlotIndex);
}

void UPRQuickbarComponent::ServerRequestUseInstance_Implementation(const FGuid InstanceGuid)
{
	RequestUseInstance(InstanceGuid);
}

void UPRQuickbarComponent::ServerCancelActiveUse_Implementation()
{
	CancelActiveUse();
}

UPRInventoryComponent* UPRQuickbarComponent::GetInventory() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
}

UPRAbilitySystemComponent* UPRQuickbarComponent::GetAbilitySystem() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
}

const FPRQuickSlotReference* UPRQuickbarComponent::FindSlot(const int32 SlotIndex) const
{
	return QuickSlots.FindByPredicate([SlotIndex](const FPRQuickSlotReference& Slot) { return Slot.SlotIndex == SlotIndex; });
}

UPRItemDataAsset* UPRQuickbarComponent::FindItemDefinition(const FGuid& InstanceGuid) const
{
	UPRInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return nullptr;
	}
	const FPRItemInstance* Item = Inventory->GetItems().FindByPredicate([InstanceGuid](const FPRItemInstance& Candidate)
	{
		return Candidate.InstanceGuid == InstanceGuid;
	});
	return Item ? Inventory->FindItemData(Item->ItemId) : nullptr;
}

bool UPRQuickbarComponent::IsSlotIndexValid(const int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < SlotCount;
}

bool UPRQuickbarComponent::IsCooldownActive(const FGameplayTag& CooldownTag) const
{
	if (!CooldownTag.IsValid() || !GetWorld())
	{
		return false;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	return Cooldowns.ContainsByPredicate([&CooldownTag, Now](const FPRQuickbarCooldownState& State)
	{
		return State.CooldownTag == CooldownTag && State.EndServerTime > Now;
	});
}

bool UPRQuickbarComponent::ValidateUse(const FPRQuickSlotReference& Slot, UPRItemDataAsset*& OutDefinition, FString& OutReason, const bool bAllowActiveUse) const
{
	OutDefinition = nullptr;
	if (!Slot.InstanceGuid.IsValid() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		OutReason = TEXT("The quickbar slot is invalid or non-authoritative.");
		return false;
	}
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRInventoryComponent* Inventory = GetInventory();
	UPRAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	APRCharacter* Character = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
	if (!Inventory || !AbilitySystem || !Character || !Character->IsAlive())
	{
		OutReason = TEXT("The player has no active combat inventory state.");
		return false;
	}
	if ((!bAllowActiveUse && ActiveUse.IsActive()) || AbilitySystem->HasMatchingGameplayTag(DownedTag()) || AbilitySystem->HasMatchingGameplayTag(DeadTag())
		|| AbilitySystem->HasMatchingGameplayTag(StunnedTag()) || AbilitySystem->HasMatchingGameplayTag(HitStaggeredTag()))
	{
		OutReason = TEXT("The player combat state blocks item use.");
		return false;
	}
	const FPRItemInstance* Item = Inventory->GetItems().FindByPredicate([&Slot](const FPRItemInstance& Candidate)
	{
		return Candidate.InstanceGuid == Slot.InstanceGuid && Candidate.Count > 0;
	});
	if (!Item)
	{
		OutReason = TEXT("The quickbar source stack is unavailable.");
		return false;
	}
	UPRItemDataAsset* Definition = Inventory->FindItemData(Item->ItemId);
	if (!Definition || !Definition->bCanUse || !Definition->UseDefinition.IsUsableFromQuickbar())
	{
		OutReason = TEXT("The source definition cannot be used from the quickbar.");
		return false;
	}
	if (IsCooldownActive(Definition->UseDefinition.CooldownTag))
	{
		OutReason = TEXT("The source item is cooling down.");
		return false;
	}
	const UPRAttributeSet* Attributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (!Attributes)
	{
		OutReason = TEXT("The player attributes are unavailable.");
		return false;
	}
	switch (Definition->UseDefinition.Kind)
	{
	case EPRItemUseKind::RestoreHealth:
		if (!Definition->UseEffect || Attributes->GetHealth() >= Attributes->GetMaxHealth()) { OutReason = TEXT("Health is already full or no health effect is configured."); return false; }
		break;
	case EPRItemUseKind::RestoreShield:
		if (!Definition->UseEffect || Attributes->GetShield() >= Attributes->GetMaxShield()) { OutReason = TEXT("Shield is already full or no shield effect is configured."); return false; }
		break;
	case EPRItemUseKind::RestoreEnergy:
		if (!Definition->UseEffect || Attributes->GetEnergy() >= Attributes->GetMaxEnergy()) { OutReason = TEXT("Energy is already full or no energy effect is configured."); return false; }
		break;
	case EPRItemUseKind::Purify:
		if (!AbilitySystem->HasMatchingGameplayTag(PollutedTag()) && !AbilitySystem->HasMatchingGameplayTag(SlowedTag())) { OutReason = TEXT("No purifiable status is active."); return false; }
		break;
	case EPRItemUseKind::GrantAmmo:
		if (Definition->UseDefinition.GrantedItemId.IsNone() || Definition->UseDefinition.GrantedItemCount <= 0
			|| Inventory->GetItemCount(Definition->UseDefinition.GrantedItemId) >= Inventory->GetMaxStackCount(Definition->UseDefinition.GrantedItemId))
		{ OutReason = TEXT("The ammunition receiver has no capacity."); return false; }
		break;
	default:
		OutReason = TEXT("The item use kind is not directly usable.");
		return false;
	}
	OutDefinition = Definition;
	return true;
}

bool UPRQuickbarComponent::BeginUseAuthoritative(const FPRQuickSlotReference& SourceReference)
{
	UPRItemDataAsset* Definition = nullptr;
	FString Reason;
	if (!ValidateUse(SourceReference, Definition, Reason))
	{
		return false;
	}
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRItemTransactionComponent* Transactions = PlayerState ? PlayerState->GetItemTransactionComponent() : nullptr;
	if (!Transactions || !GetWorld())
	{
		return false;
	}
	FPRItemTransactionRequest Request;
	Request.Header.TransactionId = FGuid::NewGuid();
	Request.Header.ExpectedRevision = Transactions->GetRevision();
	Request.Intent = EPRItemTransactionIntent::BeginUse;
	Request.InstanceGuid = SourceReference.InstanceGuid;
	Request.Count = 1;
	const FPRItemTransactionResult Result = Transactions->ExecuteAuthoritativeTransaction(Request);
	if (Result.Status != EPRItemTransactionStatus::Pending)
	{
		return false;
	}
	ActiveUse.TransactionId = Request.Header.TransactionId;
	ActiveUse.InstanceGuid = SourceReference.InstanceGuid;
	ActiveUse.SlotIndex = SourceReference.SlotIndex;
	ActiveUse.StartedServerTime = GetWorld()->GetTimeSeconds();
	ActiveUse.DurationSeconds = Definition->UseDefinition.UseDurationSeconds;
	ActiveUse.Kind = Definition->UseDefinition.Kind;
	if (UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent())
	{
		Weapon->SetAiming(false);
		Weapon->CancelReload();
	}
	SetUsingTag(true);
	GetWorld()->GetTimerManager().SetTimer(ActiveUseTimerHandle, this, &UPRQuickbarComponent::CompleteActiveUse, FMath::Max(0.0f, ActiveUse.DurationSeconds), false);
	BroadcastChanged();
	return true;
}

void UPRQuickbarComponent::CompleteActiveUse()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ActiveUse.IsActive())
	{
		return;
	}
	const FPRQuickbarUseState CompletingUse = ActiveUse;
	FPRQuickSlotReference SourceReference;
	SourceReference.SlotIndex = CompletingUse.SlotIndex;
	SourceReference.InstanceGuid = CompletingUse.InstanceGuid;
	const FPRQuickSlotReference* Slot = IsSlotIndexValid(CompletingUse.SlotIndex) ? FindSlot(CompletingUse.SlotIndex) : nullptr;
	UPRItemDataAsset* Definition = nullptr;
	FString Reason;
	if ((Slot && Slot->InstanceGuid != CompletingUse.InstanceGuid) || !ValidateUse(SourceReference, Definition, Reason, true))
	{
		CancelActiveUseAuthoritative(TEXT("Quickbar use no longer satisfies completion validation."));
		return;
	}
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRItemTransactionComponent* Transactions = PlayerState ? PlayerState->GetItemTransactionComponent() : nullptr;
	UPRAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!Transactions || !AbilitySystem)
	{
		CancelActiveUseAuthoritative(TEXT("Quickbar completion lost authoritative services."));
		return;
	}
	int32 GrantCount = 0;
	FName GrantItemId = NAME_None;
	if (Definition->UseDefinition.Kind == EPRItemUseKind::GrantAmmo)
	{
		UPRInventoryComponent* Inventory = GetInventory();
		GrantItemId = Definition->UseDefinition.GrantedItemId;
		GrantCount = Inventory ? FMath::Min(Definition->UseDefinition.GrantedItemCount,
			FMath::Max(0, Inventory->GetMaxStackCount(GrantItemId) - Inventory->GetItemCount(GrantItemId))) : 0;
		if (GrantCount <= 0)
		{
			CancelActiveUseAuthoritative(TEXT("The ammunition receiver became full during use."));
			return;
		}
	}
	const FPRItemTransactionResult TransactionResult = Transactions->CompletePendingUse(CompletingUse.TransactionId, GrantItemId, GrantCount);
	if (TransactionResult.Status != EPRItemTransactionStatus::Success)
	{
		CancelActiveUseAuthoritative(TEXT("The pending quickbar transaction did not complete."));
		return;
	}
	if (Definition->UseDefinition.Kind == EPRItemUseKind::Purify)
	{
		UPRCombatEffectLibrary::ClearPurifiableStatusEffects(AbilitySystem);
	}
	else if (Definition->UseDefinition.Kind != EPRItemUseKind::GrantAmmo)
	{
		const UGameplayEffect* Effect = Definition->UseEffect ? Definition->UseEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (Effect)
		{
			FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
			Context.AddSourceObject(this);
			AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.0f, Context);
		}
	}
	StartCooldown(Definition->UseDefinition);
	SetUsingTag(false);
	ActiveUse = FPRQuickbarUseState();
	ReconcileInventoryReferences();
	BroadcastChanged();
}

void UPRQuickbarComponent::CancelActiveUseAuthoritative(const FString& Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ActiveUseTimerHandle);
	}
	if (ActiveUse.IsActive())
	{
		if (APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner()))
		{
			if (UPRItemTransactionComponent* Transactions = PlayerState->GetItemTransactionComponent())
			{
				Transactions->CancelPendingUse(ActiveUse.TransactionId, Reason);
			}
		}
	}
	SetUsingTag(false);
	ActiveUse = FPRQuickbarUseState();
	BroadcastChanged();
}

void UPRQuickbarComponent::SetUsingTag(const bool bEnabled)
{
	if (UPRAbilitySystemComponent* AbilitySystem = GetAbilitySystem(); AbilitySystem && UsingItemTag().IsValid())
	{
		if (bEnabled) { AbilitySystem->AddLooseGameplayTag(UsingItemTag()); }
		else { AbilitySystem->RemoveLooseGameplayTag(UsingItemTag()); }
	}
}

void UPRQuickbarComponent::StartCooldown(const FPRItemUseDefinition& Definition)
{
	if (!Definition.CooldownTag.IsValid() || Definition.CooldownSeconds <= 0.0f || !GetWorld())
	{
		return;
	}
	const float EndTime = GetWorld()->GetTimeSeconds() + Definition.CooldownSeconds;
	FPRQuickbarCooldownState* Existing = Cooldowns.FindByPredicate([&Definition](const FPRQuickbarCooldownState& State)
	{
		return State.CooldownTag == Definition.CooldownTag;
	});
	if (Existing) { Existing->EndServerTime = EndTime; }
	else { Cooldowns.Emplace(FPRQuickbarCooldownState{ Definition.CooldownTag, EndTime }); }
}

void UPRQuickbarComponent::BroadcastChanged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->ForceNetUpdate();
	}
	OnQuickbarChanged.Broadcast();
}

void UPRQuickbarComponent::HandleInventoryChanged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ReconcileInventoryReferences();
	}
	else
	{
		OnQuickbarChanged.Broadcast();
	}
}

void UPRQuickbarComponent::OnRep_QuickSlots() { OnQuickbarChanged.Broadcast(); }
void UPRQuickbarComponent::OnRep_ActiveUse() { OnQuickbarChanged.Broadcast(); }
void UPRQuickbarComponent::OnRep_Cooldowns() { OnQuickbarChanged.Broadcast(); }
