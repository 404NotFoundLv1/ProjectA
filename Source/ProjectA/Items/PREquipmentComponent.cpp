#include "Items/PREquipmentComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRAffixGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "Characters/PRCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "Items/PRInventoryComponent.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PRAffixGenerationLibrary.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PREquipmentVisualActor.h"
#include "Core/PRAssetManager.h"
#include "Core/PRGameplayTags.h"
#include "Player/PRPlayerState.h"
#include "Net/UnrealNetwork.h"

namespace ProjectRiftEquipmentPrivate
{
bool IsAuthorityOrOfflineTest(const APRPlayerState* PlayerState)
{
	// Profile bridge automation uses transient PlayerStates with no UWorld.
	// They cannot receive network RPCs, so accepting their deterministic state
	// transition does not relax authority for an actual client actor.
	return PlayerState && (PlayerState->HasAuthority() || PlayerState->GetWorld() == nullptr);
}

TSubclassOf<UGameplayEffect> GetUpgradeEffectClass(const EPRAffixAttribute Attribute, const EPRAffixModifierType ModifierType)
{
	if (ModifierType == EPRAffixModifierType::Percentage)
	{
		switch (Attribute)
		{
		case EPRAffixAttribute::MaxHealth: return UPRAffixMaxHealthPercentageGameplayEffect::StaticClass();
		case EPRAffixAttribute::MaxShield: return UPRAffixMaxShieldPercentageGameplayEffect::StaticClass();
		case EPRAffixAttribute::MaxEnergy: return UPRAffixMaxEnergyPercentageGameplayEffect::StaticClass();
		case EPRAffixAttribute::AttackPower: return UPRAffixAttackPowerPercentageGameplayEffect::StaticClass();
		case EPRAffixAttribute::MoveSpeed: return UPRAffixMoveSpeedPercentageGameplayEffect::StaticClass();
		default: return nullptr;
		}
	}
	switch (Attribute)
	{
	case EPRAffixAttribute::MaxHealth: return UPRAffixMaxHealthAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::MaxShield: return UPRAffixMaxShieldAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::MaxEnergy: return UPRAffixMaxEnergyAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::AttackPower: return UPRAffixAttackPowerAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::MoveSpeed: return UPRAffixMoveSpeedAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::HealingPower: return UPRAffixHealingPowerAdditiveGameplayEffect::StaticClass();
	case EPRAffixAttribute::PollutionResistance: return UPRAffixPollutionResistanceAdditiveGameplayEffect::StaticClass();
	default: return nullptr;
	}
}
}

UPREquipmentComponent::UPREquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FPRItemInstance UPREquipmentComponent::GetEquippedItem(const FName SlotId) const
{
	const FPRProfileEquipmentEntry* Entry = EquipmentEntries.FindByPredicate([SlotId](const FPRProfileEquipmentEntry& Candidate)
	{
		return Candidate.SlotId == SlotId;
	});
	return Entry ? Entry->Item : FPRItemInstance();
}

void UPREquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClearGrantedHandles();
		DestroyVisualActors();
	}
	Super::EndPlay(EndPlayReason);
}

void UPREquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPREquipmentComponent, EquipmentEntries, COND_OwnerOnly);
	DOREPLIFETIME(UPREquipmentComponent, PublicAppearanceEntries);
}

FName UPREquipmentComponent::GetSlotId(const EPREquipmentSlot Slot)
{
	switch (Slot)
	{
	case EPREquipmentSlot::Weapon: return ProjectRiftEquipmentSlots::Weapon;
	case EPREquipmentSlot::Armor: return ProjectRiftEquipmentSlots::Armor;
	case EPREquipmentSlot::Chip: return ProjectRiftEquipmentSlots::Chip;
	case EPREquipmentSlot::Tool: return ProjectRiftEquipmentSlots::Tool;
	default: return NAME_None;
	}
}

bool UPREquipmentComponent::ReplaceEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	if (!ProjectRiftEquipmentPrivate::IsAuthorityOrOfflineTest(PlayerState))
	{
		OutDiagnostic = TEXT("Equipment can only be changed by the authoritative PlayerState.");
		return false;
	}
	if (!ValidateEquipmentEntries(InEntries, OutDiagnostic))
	{
		return false;
	}

	const TArray<FPRProfileEquipmentEntry> PreviousEntries = EquipmentEntries;
	UPRAttributeSet* AttributeSet = PlayerState->GetAttributeSet();
	const float PreviousMaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
	const float PreviousHealthFraction = PreviousMaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(AttributeSet->GetHealth() / PreviousMaxHealth, 0.0f, 1.0f)
		: 1.0f;
	ClearGrantedHandles();
	EquipmentEntries = InEntries;
	RebuildPublicAppearanceEntries();
	if (!RefreshGrantedHandles())
	{
		ClearGrantedHandles();
		EquipmentEntries = PreviousEntries;
		RebuildPublicAppearanceEntries();
		FString RestoreDiagnostic;
		if (!RefreshGrantedHandles())
		{
			OutDiagnostic = TEXT("Equipment grant replacement failed and the previous grants could not be restored.");
			return false;
		}
		OutDiagnostic = TEXT("Equipment grant replacement failed; the previous equipment state was restored.");
		return false;
	}
	if (AttributeSet && PreviousMaxHealth > KINDA_SMALL_NUMBER
		&& GetAbilitySystemComponent() && GetAbilitySystemComponent()->GetOwnerActor())
	{
		AttributeSet->SetHealth(FMath::Clamp(PreviousHealthFraction * AttributeSet->GetMaxHealth(), 0.0f, AttributeSet->GetMaxHealth()));
	}
	RefreshVisualActors();

	NotifyEquipmentChanged();
	return true;
}

bool UPREquipmentComponent::RefreshGrantedHandles()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!ProjectRiftEquipmentPrivate::IsAuthorityOrOfflineTest(PlayerState) || !AbilitySystemComponent)
	{
		return false;
	}
	if (PlayerState->GetWorld() == nullptr)
	{
		// No actor info exists on a transient bridge object. Persist the accepted
		// layout while intentionally skipping transient GAS grants.
		ClearGrantedHandles();
		return true;
	}
	if (HasMatchingGrantedHandles())
	{
		return true;
	}

	ClearGrantedHandles();
	TMap<FGuid, FPREquipmentRuntimeHandles> CandidateHandles;
	for (const FPRProfileEquipmentEntry& Entry : EquipmentEntries)
	{
		const UPREquipmentDataAsset* Definition = ResolveEquipmentDefinition(Entry.Item);
		if (!Definition)
		{
			// Unknown legacy definitions remain persisted but cannot grant runtime gameplay.
			continue;
		}
		if (Definition->EquipmentSlot == EPREquipmentSlot::None || GetSlotId(Definition->EquipmentSlot) != Entry.SlotId)
		{
			for (const TPair<FGuid, FPREquipmentRuntimeHandles>& Pair : CandidateHandles)
			{
				RemoveHandles(Pair.Value);
			}
			return false;
		}

		FPREquipmentRuntimeHandles Handles;
		if (!ApplyGrantsForEntry(Entry, Definition, Handles))
		{
			RemoveHandles(Handles);
			for (const TPair<FGuid, FPREquipmentRuntimeHandles>& Pair : CandidateHandles)
			{
				RemoveHandles(Pair.Value);
			}
			return false;
		}
		CandidateHandles.Add(Entry.Item.InstanceGuid, MoveTemp(Handles));
	}

	RuntimeHandlesByInstance = MoveTemp(CandidateHandles);
	return true;
}

void UPREquipmentComponent::ClearGrantedHandles()
{
	for (const TPair<FGuid, FPREquipmentRuntimeHandles>& Pair : RuntimeHandlesByInstance)
	{
		RemoveHandles(Pair.Value);
	}
	RuntimeHandlesByInstance.Reset();
}

void UPREquipmentComponent::CopyRuntimeStateFrom(const UPREquipmentComponent* SourceComponent)
{
	if (!SourceComponent)
	{
		return;
	}
	ClearGrantedHandles();
	EquipmentEntries = SourceComponent->EquipmentEntries;
	PublicAppearanceEntries = SourceComponent->PublicAppearanceEntries;
	RuntimeHandlesByInstance.Reset();
	NotifyEquipmentChanged();
}

void UPREquipmentComponent::HandleAvatarChanged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RefreshGrantedHandles();
		RefreshVisualActors();
	}
}

bool UPREquipmentComponent::ValidateEquipmentEntries(const TArray<FPRProfileEquipmentEntry>& InEntries, FString& OutDiagnostic) const
{
	TSet<FName> SeenSlots;
	TSet<FGuid> SeenInstances;
	for (const FPRProfileEquipmentEntry& Entry : InEntries)
	{
		const UPREquipmentDataAsset* Definition = ResolveEquipmentDefinition(Entry.Item);
		const bool bKnownDefinitionWithoutIdentity = Definition && !Entry.Item.HasValidIdentity();
		if (Entry.SlotId.IsNone() || !Entry.Item.IsValid() || bKnownDefinitionWithoutIdentity
			|| SeenSlots.Contains(Entry.SlotId)
			|| (Entry.Item.HasValidIdentity() && SeenInstances.Contains(Entry.Item.InstanceGuid)))
		{
			OutDiagnostic = TEXT("Equipment entries contain an invalid, duplicate slot, or duplicate instance identity.");
			return false;
		}
		SeenSlots.Add(Entry.SlotId);
		if (Entry.Item.HasValidIdentity())
		{
			SeenInstances.Add(Entry.Item.InstanceGuid);
		}
	}
	return true;
}

bool UPREquipmentComponent::HasMatchingGrantedHandles() const
{
	UPRAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return false;
	}
	int32 KnownDefinitionCount = 0;
	for (const FPRProfileEquipmentEntry& Entry : EquipmentEntries)
	{
		if (ResolveEquipmentDefinition(Entry.Item))
		{
			++KnownDefinitionCount;
			const FPREquipmentRuntimeHandles* Handles = RuntimeHandlesByInstance.Find(Entry.Item.InstanceGuid);
			if (!Handles)
			{
				return false;
			}
			for (const FActiveGameplayEffectHandle& Handle : Handles->ActiveEffectHandles)
			{
				if (!Handle.IsValid() || !AbilitySystemComponent->GetActiveGameplayEffect(Handle))
				{
					return false;
				}
			}
			for (const FGameplayAbilitySpecHandle& Handle : Handles->GrantedAbilityHandles)
			{
				if (!Handle.IsValid() || !AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
				{
					return false;
				}
			}
		}
	}
	return RuntimeHandlesByInstance.Num() == KnownDefinitionCount;
}

bool UPREquipmentComponent::ApplyGrantsForEntry(
	const FPRProfileEquipmentEntry& Entry,
	const UPREquipmentDataAsset* Definition,
	FPREquipmentRuntimeHandles& OutHandles)
{
	UPRAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!AbilitySystemComponent || !Definition)
	{
		return false;
	}
	for (const TSubclassOf<UGameplayEffect>& EffectClass : Definition->GrantedEffects)
	{
		const UGameplayEffect* Effect = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!Effect)
		{
			return false;
		}
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(const_cast<UPREquipmentDataAsset*>(Definition));
		const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, FMath::Max(1, Entry.Item.Level), Context);
		if (!SpecHandle.IsValid())
		{
			return false;
		}
		const FActiveGameplayEffectHandle ActiveHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (!ActiveHandle.IsValid())
		{
			return false;
		}
		OutHandles.ActiveEffectHandles.Add(ActiveHandle);
	}
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Definition->GrantedAbilities)
	{
		if (!AbilityClass)
		{
			return false;
		}
		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, FMath::Max(1, Entry.Item.Level), INDEX_NONE, const_cast<UPREquipmentDataAsset*>(Definition)));
		if (!Handle.IsValid())
		{
			return false;
		}
		OutHandles.GrantedAbilityHandles.Add(Handle);
	}
	const int32 UpgradeSteps = FMath::Max(0, Entry.Item.Level - 1);
	if (UpgradeSteps > 0)
	{
		for (const FPRUpgradeAttributeModifier& Modifier : Definition->UpgradeModifiersPerLevel)
		{
			const TSubclassOf<UGameplayEffect> EffectClass = ProjectRiftEquipmentPrivate::GetUpgradeEffectClass(Modifier.Attribute, EPRAffixModifierType::Additive);
			const float Magnitude = Modifier.MagnitudePerLevel * UpgradeSteps;
			if (!EffectClass || !FMath::IsFinite(Magnitude) || Magnitude <= 0.0f) { return false; }
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddSourceObject(const_cast<UPREquipmentDataAsset*>(Definition));
			const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.0f, Context);
			if (!Spec.IsValid()) { return false; }
			Spec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Affix_Magnitude, Magnitude);
			const FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (!Handle.IsValid()) { return false; }
			OutHandles.ActiveEffectHandles.Add(Handle);
		}
	}

	// Legacy v5 equipment did not have value rolls and therefore never receives implicit grants.
	if (Entry.Item.AffixGenerationVersion == 0 && Entry.Item.RolledAffixes.IsEmpty())
	{
		return true;
	}
	if (Entry.Item.AffixGenerationVersion != UPRAffixGenerationLibrary::CurrentGenerationVersion
		|| Entry.Item.RolledAffixes.Num() != Entry.Item.Affixes.Num())
	{
		// Preserve a future final-roll payload without inventing gameplay behavior for it.
		return Entry.Item.AffixGenerationVersion > UPRAffixGenerationLibrary::CurrentGenerationVersion;
	}
	if (Entry.Item.RolledAffixes.IsEmpty())
	{
		return true;
	}

	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TArray<UPRAffixDefinitionDataAsset*> AffixCatalog;
	if (!AssetManager || !AssetManager->LoadAffixCatalog(AffixCatalog))
	{
		return false;
	}
	for (int32 Index = 0; Index < Entry.Item.RolledAffixes.Num(); ++Index)
	{
		const FPRAffixRoll& Roll = Entry.Item.RolledAffixes[Index];
		if (!Roll.IsValid() || Entry.Item.Affixes[Index] != Roll.AffixId)
		{
			return false;
		}
		UPRAffixDefinitionDataAsset* const* DefinitionEntry = AffixCatalog.FindByPredicate([&Roll](const UPRAffixDefinitionDataAsset* Candidate)
		{
			return Candidate && Candidate->AffixId == Roll.AffixId;
		});
		const UPRAffixDefinitionDataAsset* AffixDefinition = DefinitionEntry ? *DefinitionEntry : nullptr;
		if (!AffixDefinition)
		{
			// The item remains authoritative and serializable even when this client/build lacks its definition.
			continue;
		}
		const UPRAffixModifierGameplayEffect* AffixEffect = AffixDefinition->ModifierEffectClass
			? Cast<UPRAffixModifierGameplayEffect>(AffixDefinition->ModifierEffectClass->GetDefaultObject())
			: nullptr;
		if (!AffixDefinition->AllowedSlots.Contains(Definition->EquipmentSlot)
			|| AffixDefinition->Attribute != Roll.Attribute || AffixDefinition->ModifierType != Roll.ModifierType
			|| !AffixEffect || AffixEffect->GetAffixAttribute() != Roll.Attribute
			|| AffixEffect->GetAffixModifierType() != Roll.ModifierType)
		{
			return false;
		}
		const float AppliedMagnitude = Roll.ModifierType == EPRAffixModifierType::Percentage
			? 1.0f + Roll.Magnitude
			: Roll.Magnitude;
		if (!FMath::IsFinite(AppliedMagnitude) || AppliedMagnitude <= 0.0f)
		{
			return false;
		}
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(const_cast<UPRAffixDefinitionDataAsset*>(AffixDefinition));
		const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
			AffixDefinition->ModifierEffectClass,
			FMath::Max(1, Entry.Item.Level),
			Context);
		if (!SpecHandle.IsValid())
		{
			return false;
		}
		SpecHandle.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Affix_Magnitude, AppliedMagnitude);
		const FActiveGameplayEffectHandle ActiveHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (!ActiveHandle.IsValid())
		{
			return false;
		}
		OutHandles.ActiveEffectHandles.Add(ActiveHandle);
	}
	return true;
}

void UPREquipmentComponent::RemoveHandles(const FPREquipmentRuntimeHandles& Handles)
{
	UPRAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}
	for (const FActiveGameplayEffectHandle& Handle : Handles.ActiveEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Handles.GrantedAbilityHandles)
	{
		if (Handle.IsValid() && AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
		{
			AbilitySystemComponent->CancelAbilityHandle(Handle);
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}
}

void UPREquipmentComponent::RebuildPublicAppearanceEntries()
{
	PublicAppearanceEntries.Reset();
	for (const FPRProfileEquipmentEntry& Entry : EquipmentEntries)
	{
		if (ResolveEquipmentDefinition(Entry.Item))
		{
			PublicAppearanceEntries.Add({ Entry.SlotId, Entry.Item.ItemId });
		}
	}
}

void UPREquipmentComponent::RefreshVisualActors()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	APRCharacter* Character = PlayerState ? Cast<APRCharacter>(PlayerState->GetPawn()) : nullptr;
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !Mesh || !GetWorld())
	{
		return;
	}
	TSet<FName> DesiredSlots;
	for (const FPRProfileEquipmentEntry& Entry : EquipmentEntries)
	{
		if (Entry.SlotId == ProjectRiftEquipmentSlots::Weapon)
		{
			continue;
		}
		const UPREquipmentDataAsset* Definition = ResolveEquipmentDefinition(Entry.Item);
		if (!Definition || !Definition->VisualActorClass)
		{
			continue;
		}
		DesiredSlots.Add(Entry.SlotId);
		APREquipmentVisualActor* Existing = SpawnedVisualActors.FindRef(Entry.SlotId);
		if (!Existing || Existing->GetClass() != Definition->VisualActorClass)
		{
			if (Existing)
			{
				Existing->Destroy();
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = PlayerState;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Existing = GetWorld()->SpawnActor<APREquipmentVisualActor>(Definition->VisualActorClass, FTransform::Identity, SpawnParameters);
			if (!Existing)
			{
				continue;
			}
			SpawnedVisualActors.Add(Entry.SlotId, Existing);
		}
		Existing->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Definition->AttachmentSocket);
		Existing->SetActorRelativeTransform(Definition->AttachmentTransform);
	}
	for (auto It = SpawnedVisualActors.CreateIterator(); It; ++It)
	{
		if (!DesiredSlots.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}

void UPREquipmentComponent::DestroyVisualActors()
{
	for (const TPair<FName, TObjectPtr<APREquipmentVisualActor>>& Pair : SpawnedVisualActors)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	SpawnedVisualActors.Reset();
}

void UPREquipmentComponent::NotifyEquipmentChanged()
{
	OnEquipmentChanged.Broadcast();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

UPRAbilitySystemComponent* UPREquipmentComponent::GetAbilitySystemComponent() const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
}

const UPREquipmentDataAsset* UPREquipmentComponent::ResolveEquipmentDefinition(const FPRItemInstance& Item) const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	return Inventory ? Cast<UPREquipmentDataAsset>(Inventory->FindItemData(Item.ItemId)) : nullptr;
}

void UPREquipmentComponent::OnRep_EquipmentEntries()
{
	NotifyEquipmentChanged();
}

void UPREquipmentComponent::OnRep_PublicAppearanceEntries()
{
	NotifyEquipmentChanged();
}
