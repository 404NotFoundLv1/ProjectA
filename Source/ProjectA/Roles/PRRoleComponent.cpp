#include "Roles/PRRoleComponent.h"

#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRAssetManager.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRShipLobbyGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "Roles/PRRoleDataAsset.h"
#include "Roles/PRRoleModuleDataAsset.h"

namespace
{
const FName AssaultRoleId(TEXT("Ability.Role.Assault"));
const FName LegacyAssaultRoleId(TEXT("Role.Assault"));

FGameplayTag GetInputTagForSlot(const EPRRoleModuleSlot Slot)
{
	switch (Slot)
	{
	case EPRRoleModuleSlot::Q: return ProjectRiftGameplayTags::Input_Ability_Skill_Q;
	case EPRRoleModuleSlot::E: return ProjectRiftGameplayTags::Input_Ability_Skill_E;
	case EPRRoleModuleSlot::R: return ProjectRiftGameplayTags::Input_Ability_Skill_R;
	default: return FGameplayTag();
	}
}
}

UPRRoleComponent::UPRRoleComponent()
{
	SetIsReplicatedByDefault(true);
}

void UPRRoleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPRRoleComponent, CurrentLoadout);
	DOREPLIFETIME_CONDITION(UPRRoleComponent, UnlockedRoleIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPRRoleComponent, UnlockedModuleIds, COND_OwnerOnly);
}

bool UPRRoleComponent::IsLoadoutValid(const FName RoleId, const FPRRoleLoadout& Loadout, FString* OutDiagnostic) const
{
	UPRRoleDataAsset* Role = nullptr;
	TArray<UPRRoleModuleDataAsset*> Modules;
	if (!ResolveValidatedLoadout(RoleId, Loadout, Role, Modules, OutDiagnostic))
	{
		return false;
	}
	if (!IsLoadoutOwned(RoleId, Loadout))
	{
		if (OutDiagnostic)
		{
			*OutDiagnostic = TEXT("The selected role loadout has not been permanently unlocked.");
		}
		return false;
	}
	return true;
}

EPRRoleLoadoutApplyResult UPRRoleComponent::ApplyLoadout(const FName RoleId, const FPRRoleLoadout& Loadout)

{
	return CommitLoadout(RoleId, Loadout, true);
}

EPRRoleLoadoutApplyResult UPRRoleComponent::CommitLoadout(
	const FName RoleId,
	const FPRRoleLoadout& Loadout,
	const bool bRequireLobby)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return EPRRoleLoadoutApplyResult::InternalFailure;
	}
	if (PlayerState->IsReady())
	{
		return EPRRoleLoadoutApplyResult::ReadyLocked;
	}
	if (bRequireLobby && !CanChangeLoadoutInCurrentWorld())
	{
		return EPRRoleLoadoutApplyResult::NotInLobby;
	}
	if (!UnlockedRoleIds.Contains(RoleId))
	{
		return EPRRoleLoadoutApplyResult::RoleLocked;
	}
	for (const FPRRoleModuleSlotEntry& Entry : Loadout.Entries)
	{
		if (!UnlockedModuleIds.Contains(Entry.ModuleId))
		{
			return EPRRoleLoadoutApplyResult::ModuleLocked;
		}
	}

	UPRRoleDataAsset* Role = nullptr;
	TArray<UPRRoleModuleDataAsset*> Modules;
	if (!ResolveValidatedLoadout(RoleId, Loadout, Role, Modules, nullptr))
	{
		return EPRRoleLoadoutApplyResult::InvalidLoadout;
	}

	const FPRRoleLoadout PreviousLoadout = CurrentLoadout;
	const FName PreviousRoleId = PlayerState->GetSelectedRoleModule();
	CurrentLoadout = Loadout;
	PlayerState->SetSelectedRoleModule(RoleId);
	if (!RefreshGrantedAbilities())
	{
		CurrentLoadout = PreviousLoadout;
		PlayerState->SetSelectedRoleModule(PreviousRoleId);
		return EPRRoleLoadoutApplyResult::InternalFailure;
	}
	PlayerState->ForceNetUpdate();
	return EPRRoleLoadoutApplyResult::Applied;
}

bool UPRRoleComponent::EnsureDefaultLoadoutForSelectedRole()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}

	FName RoleId = PlayerState->GetSelectedRoleModule();
	if (RoleId.IsNone() || RoleId == LegacyAssaultRoleId)
	{
		RoleId = AssaultRoleId;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> CatalogModules;
	UPRRoleDataAsset* Role = nullptr;
	if (AssetManager && AssetManager->LoadRoleCatalog(Roles, CatalogModules))
	{
		if (UPRRoleDataAsset* const* FoundRole = Roles.FindByPredicate(
			[RoleId](const UPRRoleDataAsset* Candidate) { return Candidate && Candidate->RoleId == RoleId; }))
		{
			Role = *FoundRole;
		}
	}
	if (!Role)
	{
		return false;
	}

	UPRRoleDataAsset* ValidatedRole = nullptr;
	TArray<UPRRoleModuleDataAsset*> Modules;
	if (!ResolveValidatedLoadout(RoleId, Role->DefaultLoadout, ValidatedRole, Modules, nullptr))
	{
		return false;
	}

	const TArray<FName> PreviousUnlockedRoles = UnlockedRoleIds;
	const TArray<FName> PreviousUnlockedModules = UnlockedModuleIds;
	const FPRRoleLoadout PreviousLoadout = CurrentLoadout;
	const FName PreviousSelectedRole = PlayerState->GetSelectedRoleModule();
	AddUnlockedId(UnlockedRoleIds, RoleId);
	for (const UPRRoleModuleDataAsset* Module : Modules)
	{
		AddUnlockedId(UnlockedModuleIds, Module ? Module->ModuleId : NAME_None);
	}

	const EPRRoleLoadoutApplyResult Result = CommitLoadout(RoleId, Role->DefaultLoadout, false);
	if (Result != EPRRoleLoadoutApplyResult::Applied)
	{
		UnlockedRoleIds = PreviousUnlockedRoles;
		UnlockedModuleIds = PreviousUnlockedModules;
		CurrentLoadout = PreviousLoadout;
		PlayerState->SetSelectedRoleModule(PreviousSelectedRole);
		return false;
	}
	return true;
}

bool UPRRoleComponent::RefreshGrantedAbilities()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !AbilitySystemComponent)
	{
		return false;
	}

	UPRRoleDataAsset* Role = nullptr;
	TArray<UPRRoleModuleDataAsset*> Modules;
	if (!IsLoadoutValid(PlayerState->GetSelectedRoleModule(), CurrentLoadout)
		|| !ResolveValidatedLoadout(PlayerState->GetSelectedRoleModule(), CurrentLoadout, Role, Modules, nullptr))
	{
		return false;
	}
	if (HasMatchingGrantedSpecs(Modules))
	{
		return RefreshRoleEnergyRegeneration(Role);
	}

	TArray<FGameplayAbilitySpecHandle> NewHandles;
	NewHandles.Reserve(Modules.Num());
	for (const UPRRoleModuleDataAsset* Module : Modules)
	{
		const FGameplayTag InputTag = Module ? GetInputTagForSlot(Module->Slot) : FGameplayTag();
		if (!Module || !Module->GameplayAbilityClass || !InputTag.IsValid())
		{
			for (const FGameplayAbilitySpecHandle& Handle : NewHandles)
			{
				AbilitySystemComponent->ClearAbility(Handle);
			}
			return false;
		}
		FGameplayAbilitySpec Spec(Module->GameplayAbilityClass, 1, INDEX_NONE, const_cast<UPRRoleModuleDataAsset*>(Module));
		Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(Spec);
		if (!Handle.IsValid())
		{
			for (const FGameplayAbilitySpecHandle& NewHandle : NewHandles)
			{
				AbilitySystemComponent->ClearAbility(NewHandle);
			}
			return false;
		}
		NewHandles.Add(Handle);
	}

	// Do not disturb the live role until both the candidate ability set and its
	// regeneration effect have been accepted.  CommitLoadout can restore its
	// data fields on failure, but the ASC must remain consistent as well.
	if (!RefreshRoleEnergyRegeneration(Role))
	{
		for (const FGameplayAbilitySpecHandle& NewHandle : NewHandles)
		{
			AbilitySystemComponent->ClearAbility(NewHandle);
		}
		return false;
	}

	const TArray<FGameplayAbilitySpecHandle> PreviousHandles = GrantedAbilityHandles;
	for (const FGameplayAbilitySpecHandle& PreviousHandle : PreviousHandles)
	{
		if (IsOwnedGrantedSpec(PreviousHandle))
		{
			AbilitySystemComponent->ClearAbility(PreviousHandle);
		}
	}
	GrantedAbilityHandles = MoveTemp(NewHandles);
	return true;
}

void UPRRoleComponent::ClearGrantedAbilities()
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		if (EnergyRegenerationHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(EnergyRegenerationHandle);
		}
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			if (IsOwnedGrantedSpec(Handle))
			{
				AbilitySystemComponent->ClearAbility(Handle);
			}
		}
	}
	GrantedAbilityHandles.Reset();
	EnergyRegenerationHandle.Invalidate();
}

void UPRRoleComponent::CopyRuntimeStateFrom(const UPRRoleComponent* SourceComponent)
{
	if (!SourceComponent)
	{
		return;
	}
	CurrentLoadout = SourceComponent->CurrentLoadout;
	UnlockedRoleIds = SourceComponent->UnlockedRoleIds;
	UnlockedModuleIds = SourceComponent->UnlockedModuleIds;
	GrantedAbilityHandles.Reset();
	EnergyRegenerationHandle.Invalidate();
}

void UPRRoleComponent::CaptureProfileRoleState(FName& OutSelectedRoleId, TArray<FName>& OutUnlockedRoles, FPRRoleLoadout& OutLoadout, TArray<FName>& OutUnlockedModules) const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	OutSelectedRoleId = PlayerState ? PlayerState->GetSelectedRoleModule() : NAME_None;
	OutUnlockedRoles = UnlockedRoleIds;
	OutLoadout = CurrentLoadout;
	OutUnlockedModules = UnlockedModuleIds;
}

void UPRRoleComponent::ApplyProfileRoleState(FName SelectedRoleId, const TArray<FName>& InUnlockedRoles, const FPRRoleLoadout& InLoadout, const TArray<FName>& InUnlockedModules)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return;
	}
	ClearGrantedAbilities();
	UnlockedRoleIds = InUnlockedRoles;
	UnlockedModuleIds = InUnlockedModules;
	CurrentLoadout = InLoadout;
	PlayerState->SetSelectedRoleModule(SelectedRoleId);
	if (IsLoadoutValid(SelectedRoleId, CurrentLoadout))
	{
		RefreshGrantedAbilities();
	}
	PlayerState->ForceNetUpdate();
}

bool UPRRoleComponent::ResolveValidatedLoadout(
	const FName RoleId,
	const FPRRoleLoadout& Loadout,
	UPRRoleDataAsset*& OutRole,
	TArray<UPRRoleModuleDataAsset*>& OutModules,
	FString* OutDiagnostic) const
{
	OutRole = nullptr;
	OutModules.Reset();
	if (!Loadout.IsStructurallyValid(OutDiagnostic))
	{
		return false;
	}
	UPRAssetManager* AssetManager = UPRAssetManager::Get();
	if (!AssetManager)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("ProjectRift AssetManager is unavailable."); }
		return false;
	}
	TArray<UPRRoleDataAsset*> Roles;
	TArray<UPRRoleModuleDataAsset*> CatalogModules;
	if (!AssetManager->LoadRoleCatalog(Roles, CatalogModules))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Role catalog failed validation."); }
		return false;
	}
	if (UPRRoleDataAsset* const* FoundRole = Roles.FindByPredicate(
		[RoleId](const UPRRoleDataAsset* Role) { return Role && Role->RoleId == RoleId; }))
	{
		OutRole = *FoundRole;
	}
	if (!OutRole)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Selected role is unknown."); }
		return false;
	}
	for (const FPRRoleModuleSlotEntry& Entry : Loadout.Entries)
	{
		UPRRoleModuleDataAsset* const* FoundModule = CatalogModules.FindByPredicate(
			[&Entry](const UPRRoleModuleDataAsset* Module)
			{
				return Module && Module->ModuleId == Entry.ModuleId;
			});
		if (!FoundModule || !*FoundModule || (*FoundModule)->RoleId != RoleId || (*FoundModule)->Slot != Entry.Slot
			|| !OutRole->AllowedModuleIds.Contains(Entry.ModuleId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Loadout module has an invalid role, slot, or allow-list entry."); }
			OutRole = nullptr;
			OutModules.Reset();
			return false;
		}
		OutModules.Add(*FoundModule);
	}
	return OutModules.Num() == 3;
}

bool UPRRoleComponent::IsLoadoutOwned(const FName RoleId, const FPRRoleLoadout& Loadout) const
{
	if (!UnlockedRoleIds.Contains(RoleId))
	{
		return false;
	}
	for (const FPRRoleModuleSlotEntry& Entry : Loadout.Entries)
	{
		if (!UnlockedModuleIds.Contains(Entry.ModuleId))
		{
			return false;
		}
	}
	return true;
}

bool UPRRoleComponent::CanChangeLoadoutInCurrentWorld() const
{
	const UWorld* World = GetWorld();
	const AGameModeBase* AuthGameMode = World ? World->GetAuthGameMode() : nullptr;
	return AuthGameMode && AuthGameMode->IsA<APRShipLobbyGameMode>();
}

bool UPRRoleComponent::IsOwnedGrantedSpec(const FGameplayAbilitySpecHandle& Handle) const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	const FGameplayAbilitySpec* Spec = AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;
	return Spec && (Spec->SourceObject.Get() && Spec->SourceObject.Get()->IsA<UPRRoleModuleDataAsset>());
}

bool UPRRoleComponent::HasMatchingGrantedSpecs(const TArray<UPRRoleModuleDataAsset*>& Modules) const
{
	if (GrantedAbilityHandles.Num() != Modules.Num())
	{
		return false;
	}
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}
	for (const UPRRoleModuleDataAsset* Module : Modules)
	{
		const FGameplayTag InputTag = Module ? GetInputTagForSlot(Module->Slot) : FGameplayTag();
		const bool bFound = GrantedAbilityHandles.ContainsByPredicate(
			[AbilitySystemComponent, Module, InputTag](const FGameplayAbilitySpecHandle& Handle)
			{
				const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
				return Spec && Spec->SourceObject.Get() == Module && Spec->Ability && Module
					&& Spec->Ability->GetClass() == Module->GameplayAbilityClass
					&& Spec->GetDynamicSpecSourceTags().HasTagExact(InputTag);
			});
		if (!bFound)
		{
			return false;
		}
	}
	return true;
}

void UPRRoleComponent::AddUnlockedId(TArray<FName>& Ids, const FName Id)
{
	if (!Id.IsNone() && !Ids.Contains(Id))
	{
		Ids.Add(Id);
	}
}

bool UPRRoleComponent::RefreshRoleEnergyRegeneration(const UPRRoleDataAsset* Role)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* AbilitySystemComponent = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !AbilitySystemComponent || !Role
		|| !FMath::IsFinite(Role->EnergyRegenPerSecond) || Role->EnergyRegenPerSecond <= 0.0f)
	{
		return false;
	}
	// Profile binding may restore the loadout before this PlayerState's ASC has ActorInfo.
	// Treat regeneration as deferred in that narrow window; the subsequent pawn/ASC refresh applies it.
	if (!AbilitySystemComponent->GetOwnerActor())
	{
		return true;
	}
	FGameplayEffectSpecHandle RegenSpec = AbilitySystemComponent->MakeOutgoingSpec(
		UPRRoleEnergyRegenGameplayEffect::StaticClass(),
		1.0f,
		AbilitySystemComponent->MakeEffectContext());
	if (!RegenSpec.IsValid())
	{
		return false;
	}
	RegenSpec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyRegen, Role->EnergyRegenPerSecond);
	const FActiveGameplayEffectHandle NewEnergyRegenerationHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*RegenSpec.Data.Get());
	if (!NewEnergyRegenerationHandle.IsValid())
	{
		return false;
	}

	const FActiveGameplayEffectHandle PreviousEnergyRegenerationHandle = EnergyRegenerationHandle;
	EnergyRegenerationHandle = NewEnergyRegenerationHandle;
	if (PreviousEnergyRegenerationHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(PreviousEnergyRegenerationHandle);
	}
	return true;
}
