#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "Roles/PRRoleTypes.h"
#include "PRRoleComponent.generated.h"

class UGameplayAbility;
class UPRRoleDataAsset;
class UPRRoleModuleDataAsset;

UCLASS(ClassGroup = (ProjectRift), meta = (BlueprintSpawnableComponent))
class PROJECTA_API UPRRoleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRRoleComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FPRRoleLoadout& GetCurrentLoadout() const { return CurrentLoadout; }
	const TArray<FName>& GetUnlockedRoleIds() const { return UnlockedRoleIds; }
	const TArray<FName>& GetUnlockedModuleIds() const { return UnlockedModuleIds; }
	bool IsLoadoutValid(FName RoleId, const FPRRoleLoadout& Loadout, FString* OutDiagnostic = nullptr) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Roles")
	EPRRoleLoadoutApplyResult ApplyLoadout(FName RoleId, const FPRRoleLoadout& Loadout);

	bool EnsureDefaultLoadoutForSelectedRole();
	bool RefreshGrantedAbilities();
	void ClearGrantedAbilities();
	void CopyRuntimeStateFrom(const UPRRoleComponent* SourceComponent);
	void CaptureProfileRoleState(FName& OutSelectedRoleId, TArray<FName>& OutUnlockedRoles, FPRRoleLoadout& OutLoadout, TArray<FName>& OutUnlockedModules) const;
	void ApplyProfileRoleState(FName SelectedRoleId, const TArray<FName>& InUnlockedRoles, const FPRRoleLoadout& InLoadout, const TArray<FName>& InUnlockedModules);

private:
	bool ResolveValidatedLoadout(
		FName RoleId,
		const FPRRoleLoadout& Loadout,
		UPRRoleDataAsset*& OutRole,
		TArray<UPRRoleModuleDataAsset*>& OutModules,
		FString* OutDiagnostic) const;
	EPRRoleLoadoutApplyResult CommitLoadout(FName RoleId, const FPRRoleLoadout& Loadout, bool bRequireLobby);
	bool IsLoadoutOwned(FName RoleId, const FPRRoleLoadout& Loadout) const;
	bool CanChangeLoadoutInCurrentWorld() const;
	bool IsOwnedGrantedSpec(const FGameplayAbilitySpecHandle& Handle) const;
	bool HasMatchingGrantedSpecs(const TArray<UPRRoleModuleDataAsset*>& Modules) const;
	bool RefreshRoleEnergyRegeneration(const UPRRoleDataAsset* Role);
	void AddUnlockedId(TArray<FName>& Ids, FName Id);

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Roles")
	FPRRoleLoadout CurrentLoadout;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Roles")
	TArray<FName> UnlockedRoleIds;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Roles")
	TArray<FName> UnlockedModuleIds;

	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	FActiveGameplayEffectHandle EnergyRegenerationHandle;
};
