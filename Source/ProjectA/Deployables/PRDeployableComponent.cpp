#include "Deployables/PRDeployableComponent.h"

#include "Deployables/PRDeployableActor.h"
#include "Abilities/PRAbilitySystemComponent.h"
#include "Abilities/PRAttributeSet.h"
#include "Abilities/PRRoleModuleGameplayEffects.h"
#include "Core/PRGameplayTags.h"
#include "Core/PRShipLobbyGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/PRPlayerState.h"
#include "Roles/PREngineerModuleDataAsset.h"
#include "Roles/PRRoleComponent.h"
#include "Weapons/PRWeaponComponent.h"

UPRDeployableComponent::UPRDeployableComponent()
{
	SetIsReplicatedByDefault(true);
}

void UPRDeployableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllDeployables();
	Super::EndPlay(EndPlayReason);
}

void UPRDeployableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPRDeployableComponent, PlacementState, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPRDeployableComponent, LastPlacementFailure, COND_OwnerOnly);
	DOREPLIFETIME(UPRDeployableComponent, ActiveSentry);
	DOREPLIFETIME(UPRDeployableComponent, ActiveRepairDrone);
	DOREPLIFETIME(UPRDeployableComponent, ActiveShieldGenerator);
}

bool UPRDeployableComponent::IsFinitePlacementTransform(const FTransform& Transform)
{
	const FVector Location = Transform.GetLocation();
	const FVector Scale = Transform.GetScale3D();
	const FQuat Rotation = Transform.GetRotation();
	return FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) && FMath::IsFinite(Location.Z)
		&& Transform.GetRotation().IsNormalized()
		&& FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y)
		&& FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W)
		&& FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z);
}

bool UPRDeployableComponent::CanBeginPlacement(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic) const
{
	const APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	const UPRRoleComponent* RoleComponent = PlayerState ? PlayerState->GetRoleComponent() : nullptr;
	const UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	const UPRAttributeSet* Attributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PlayerState || !RoleComponent || !ASC || !Attributes || !Module || !Module->ValidateDefinition())
	{
		OutDiagnostic = TEXT("Deployment module state is unavailable or invalid.");
		return false;
	}
	if (Cast<APRShipLobbyGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
	{
		OutDiagnostic = TEXT("Deployables are unavailable in the ship lobby.");
		return false;
	}
	if (PlayerState->GetSelectedRoleModule() != TEXT("Ability.Role.Engineer")
		|| !RoleComponent->GetUnlockedModuleIds().Contains(Module->ModuleId)
		|| !RoleComponent->GetCurrentLoadout().Entries.ContainsByPredicate([Module](const FPRRoleModuleSlotEntry& Entry) { return Entry.ModuleId == Module->ModuleId; }))
	{
		OutDiagnostic = TEXT("Deployment module is not equipped by the active Engineer role.");
		return false;
	}
	if (ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Downed)
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Reviving)
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_BeingRevived)
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_Stunned)
		|| ASC->HasMatchingGameplayTag(ProjectRiftGameplayTags::State_HitStaggered)
		|| Attributes->GetEnergy() < Module->EnergyCost)
	{
		OutDiagnostic = TEXT("Current combat state cannot begin a deployment.");
		return false;
	}
	const FGameplayTag CooldownTag = Module->Slot == EPRRoleModuleSlot::Q ? ProjectRiftGameplayTags::Cooldown_Skill_Q
		: Module->Slot == EPRRoleModuleSlot::E ? ProjectRiftGameplayTags::Cooldown_Skill_E
		: Module->Slot == EPRRoleModuleSlot::R ? ProjectRiftGameplayTags::Cooldown_Skill_R : FGameplayTag();
	if (!CooldownTag.IsValid() || ASC->HasMatchingGameplayTag(CooldownTag))
	{
		OutDiagnostic = TEXT("Deployment module is cooling down.");
		return false;
	}
	return true;
}

bool UPRDeployableComponent::BeginPlacement(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic)
{
	if (!CanBeginPlacement(Module, OutDiagnostic))
	{
		LastPlacementFailure = OutDiagnostic;
		return false;
	}
	if (PlacementState.bPending)
	{
		CancelPlacement(TEXT("Replaced by a new deployment module."));
	}
	PendingModule = Module;
	PlacementState.ModuleId = Module->ModuleId;
	PlacementState.DeployableKind = Module->DeployableKind;
	PlacementState.SessionSequence = FMath::Max(1, PlacementState.SessionSequence + 1);
	PlacementState.ExpiresAtServerTime = GetWorld()->GetTimeSeconds() + 8.0f;
	PlacementState.bPending = true;
	LastPlacementFailure.Reset();
	SetPlacementStateActive(true);
	if (APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner()))
	{
		if (UPRWeaponComponent* Weapon = PlayerState->GetWeaponComponent())
		{
			Weapon->CancelReload();
			Weapon->SetAiming(false);
		}
	}
	GetWorld()->GetTimerManager().SetTimer(PlacementTimeoutHandle, this, &UPRDeployableComponent::HandlePlacementTimeout, 8.0f, false);
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UPRDeployableComponent::ResolveAuthoritativePlacement(
	APlayerController* Controller,
	const UPREngineerModuleDataAsset* Module,
	const FTransform& ClientTransform,
	FTransform& OutTransform,
	EPRDeployablePlacementResult& OutResult,
	FString& OutDiagnostic) const
{
	OutResult = EPRDeployablePlacementResult::InternalFailure;
	if (!Controller || !Module || !IsFinitePlacementTransform(ClientTransform) || !GetWorld() || !Controller->GetPawn())
	{
		OutResult = EPRDeployablePlacementResult::InvalidTransform;
		OutDiagnostic = TEXT("Deployment transform is invalid.");
		return false;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewEnd = ViewLocation + ViewRotation.Vector().GetSafeNormal() * Module->PlacementRange;
	FCollisionObjectQueryParams WorldObjects;
	WorldObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	WorldObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PRDeployablePlacement), false, Controller->GetPawn());
	FHitResult SurfaceHit;
	if (!GetWorld()->LineTraceSingleByObjectType(SurfaceHit, ViewLocation, ViewEnd, WorldObjects, TraceParams) || !SurfaceHit.bBlockingHit)
	{
		OutResult = EPRDeployablePlacementResult::InvalidSurface;
		OutDiagnostic = TEXT("Deployment requires a visible world surface.");
		return false;
	}
	const FVector PlacementLocation = SurfaceHit.ImpactPoint;
	if (FVector::DistSquared(PlacementLocation, ClientTransform.GetLocation()) > FMath::Square(75.0f)
		|| FVector::DistSquared(Controller->GetPawn()->GetActorLocation(), PlacementLocation) > FMath::Square(Module->PlacementRange))
	{
		OutResult = EPRDeployablePlacementResult::OutOfRange;
		OutDiagnostic = TEXT("Deployment position differs from the authoritative placement point.");
		return false;
	}
	const float MinUpDot = FMath::Cos(FMath::DegreesToRadians(Module->MaxSurfaceSlopeDegrees));
	if (FVector::DotProduct(SurfaceHit.ImpactNormal.GetSafeNormal(), FVector::UpVector) < MinUpDot)
	{
		OutResult = EPRDeployablePlacementResult::InvalidSurface;
		OutDiagnostic = TEXT("Deployment surface is too steep.");
		return false;
	}
	FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(PRDeployablePlacementLOS), false, Controller->GetPawn());
	FHitResult VisibilityHit;
	if (GetWorld()->LineTraceSingleByChannel(VisibilityHit, Controller->GetPawn()->GetActorLocation(), PlacementLocation, ECC_Visibility, VisibilityParams)
		&& VisibilityHit.GetActor() != SurfaceHit.GetActor())
	{
		OutResult = EPRDeployablePlacementResult::Obstructed;
		OutDiagnostic = TEXT("Deployment point is blocked from the player.");
		return false;
	}
	if (GetWorld()->OverlapAnyTestByObjectType(PlacementLocation + SurfaceHit.ImpactNormal * 5.0f, FQuat::Identity, WorldObjects, FCollisionShape::MakeSphere(Module->FootprintRadius), TraceParams))
	{
		OutResult = EPRDeployablePlacementResult::Occupied;
		OutDiagnostic = TEXT("Deployment footprint is occupied.");
		return false;
	}
	OutTransform = FTransform(FRotator(0.0f, ViewRotation.Yaw, 0.0f), PlacementLocation + SurfaceHit.ImpactNormal * 5.0f);
	OutResult = EPRDeployablePlacementResult::Confirmed;
	return true;
}

bool UPRDeployableComponent::CommitModuleCostAndCooldown(const UPREngineerModuleDataAsset* Module, FString& OutDiagnostic) const
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	UPRAttributeSet* Attributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (!ASC || !Attributes || !Module || Attributes->GetEnergy() < Module->EnergyCost)
	{
		OutDiagnostic = TEXT("Deployment energy is no longer available.");
		return false;
	}
	const FGameplayTag CooldownTag = Module->Slot == EPRRoleModuleSlot::Q ? ProjectRiftGameplayTags::Cooldown_Skill_Q
		: Module->Slot == EPRRoleModuleSlot::E ? ProjectRiftGameplayTags::Cooldown_Skill_E
		: Module->Slot == EPRRoleModuleSlot::R ? ProjectRiftGameplayTags::Cooldown_Skill_R : FGameplayTag();
	const TSubclassOf<UGameplayEffect> CooldownClass = Module->Slot == EPRRoleModuleSlot::Q ? UPRAssaultChargeCooldownGameplayEffect::StaticClass()
		: Module->Slot == EPRRoleModuleSlot::E ? UPRAssaultBlastCooldownGameplayEffect::StaticClass()
		: Module->Slot == EPRRoleModuleSlot::R ? UPRAssaultShieldCooldownGameplayEffect::StaticClass() : nullptr;
	if (!CooldownTag.IsValid() || !CooldownClass || ASC->HasMatchingGameplayTag(CooldownTag))
	{
		OutDiagnostic = TEXT("Deployment cooldown is no longer available.");
		return false;
	}
	FGameplayEffectSpecHandle CostSpec = ASC->MakeOutgoingSpec(UPRRoleModuleCostGameplayEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	FGameplayEffectSpecHandle CooldownSpec = ASC->MakeOutgoingSpec(CooldownClass, 1.0f, ASC->MakeEffectContext());
	if (!CostSpec.IsValid() || !CooldownSpec.IsValid())
	{
		OutDiagnostic = TEXT("Deployment GameplayEffect specifications could not be created.");
		return false;
	}
	CostSpec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_EnergyDelta, -Module->EnergyCost);
	const float CooldownDuration = Module->CooldownSeconds * (1.0f - FMath::Clamp(Attributes->GetCooldownReduction(), 0.0f, 0.8f));
	CooldownSpec.Data->SetSetByCallerMagnitude(ProjectRiftGameplayTags::Data_Ability_CooldownDuration, CooldownDuration);
	ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
	ASC->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
	return true;
}

EPRDeployablePlacementResult UPRDeployableComponent::ConfirmPlacement(
	APlayerController* Controller,
	const FTransform& ClientTransform,
	const int32 SessionSequence,
	FString& OutDiagnostic)
{
	if (!PlacementState.bPending || !PendingModule || SessionSequence != PlacementState.SessionSequence || bConfirmInFlight)
	{
		OutDiagnostic = TEXT("Deployment confirmation is stale or duplicated.");
		LastPlacementFailure = OutDiagnostic;
		return EPRDeployablePlacementResult::Duplicate;
	}
	if (GetWorld()->GetTimeSeconds() > PlacementState.ExpiresAtServerTime)
	{
		CancelPlacement(TEXT("Deployment preview expired."));
		OutDiagnostic = LastPlacementFailure;
		return EPRDeployablePlacementResult::Expired;
	}
	bConfirmInFlight = true;
	FTransform AuthoritativeTransform;
	EPRDeployablePlacementResult ValidationResult = EPRDeployablePlacementResult::InternalFailure;
	const bool bCanStillPlace = CanBeginPlacement(PendingModule, OutDiagnostic);
	if (!bCanStillPlace
		|| !ResolveAuthoritativePlacement(Controller, PendingModule, ClientTransform, AuthoritativeTransform, ValidationResult, OutDiagnostic))
	{
		bConfirmInFlight = false;
		LastPlacementFailure = OutDiagnostic;
		return bCanStillPlace ? ValidationResult : EPRDeployablePlacementResult::InvalidState;
	}
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Controller;
	SpawnParams.Instigator = Controller ? Controller->GetPawn() : nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APRDeployableActor* NewDeployable = GetWorld()->SpawnActorDeferred<APRDeployableActor>(PendingModule->DeployableActorClass, AuthoritativeTransform, Controller, Controller ? Controller->GetPawn() : nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewDeployable || !CommitModuleCostAndCooldown(PendingModule, OutDiagnostic))
	{
		if (NewDeployable) { NewDeployable->Destroy(); }
		bConfirmInFlight = false;
		LastPlacementFailure = OutDiagnostic.IsEmpty() ? TEXT("Deployment could not be committed.") : OutDiagnostic;
		return EPRDeployablePlacementResult::InternalFailure;
	}
	NewDeployable->InitializeDeployable(PlayerState, PendingModule->DeployableKind, PendingModule->LifetimeSeconds);
	NewDeployable->FinishSpawning(AuthoritativeTransform);
	NewDeployable->ConfigureFromModule(PendingModule);
	if (APRDeployableActor* Previous = GetActiveDeployable(PendingModule->DeployableKind))
	{
		Previous->Destroy();
	}
	SetActiveDeployable(PendingModule->DeployableKind, NewDeployable);
	CancelPlacement();
	bConfirmInFlight = false;
	return EPRDeployablePlacementResult::Confirmed;
}

void UPRDeployableComponent::CancelPlacement(const FString& Reason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PlacementTimeoutHandle);
	}
	PendingModule = nullptr;
	PlacementState.ModuleId = NAME_None;
	PlacementState.DeployableKind = EPRDeployableKind::None;
	PlacementState.ExpiresAtServerTime = 0.0f;
	PlacementState.bPending = false;
	if (!Reason.IsEmpty()) { LastPlacementFailure = Reason; }
	SetPlacementStateActive(false);
	if (GetOwner()) { GetOwner()->ForceNetUpdate(); }
}

void UPRDeployableComponent::SetPlacementStateActive(const bool bActive)
{
	APRPlayerState* PlayerState = Cast<APRPlayerState>(GetOwner());
	UPRAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetProjectRiftAbilitySystemComponent() : nullptr;
	if (ASC)
	{
		if (bActive) { ASC->AddLooseGameplayTag(ProjectRiftGameplayTags::State_PlacingDeployable); }
		else { ASC->RemoveLooseGameplayTag(ProjectRiftGameplayTags::State_PlacingDeployable); }
	}
}

void UPRDeployableComponent::SetActiveDeployable(const EPRDeployableKind Kind, APRDeployableActor* Deployable)
{
	switch (Kind)
	{
	case EPRDeployableKind::Sentry: ActiveSentry = Deployable; break;
	case EPRDeployableKind::RepairDrone: ActiveRepairDrone = Deployable; break;
	case EPRDeployableKind::ShieldGenerator: ActiveShieldGenerator = Deployable; break;
	default: break;
	}
	if (GetOwner()) { GetOwner()->ForceNetUpdate(); }
}

void UPRDeployableComponent::HandlePlacementTimeout()
{
	CancelPlacement(TEXT("Deployment preview expired."));
}

APRDeployableActor* UPRDeployableComponent::GetActiveDeployable(const EPRDeployableKind Kind) const
{
	switch (Kind)
	{
	case EPRDeployableKind::Sentry: return ActiveSentry;
	case EPRDeployableKind::RepairDrone: return ActiveRepairDrone;
	case EPRDeployableKind::ShieldGenerator: return ActiveShieldGenerator;
	default: return nullptr;
	}
}

void UPRDeployableComponent::ClearAllDeployables()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	CancelPlacement();
	for (APRDeployableActor* Deployable : { ActiveSentry.Get(), ActiveRepairDrone.Get(), ActiveShieldGenerator.Get() })
	{
		if (Deployable)
		{
			Deployable->Destroy();
		}
	}
	ActiveSentry = nullptr;
	ActiveRepairDrone = nullptr;
	ActiveShieldGenerator = nullptr;
}
