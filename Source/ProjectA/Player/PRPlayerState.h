#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "PRPlayerState.generated.h"

class UPRAbilitySystemComponent;
class UPRAttributeSet;
class UAbilitySystemComponent;
class UPRInventoryComponent;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRShipResourceStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ship|Resources")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Ship|Resources", meta = (ClampMin = "0"))
	int32 Count = 0;

	bool IsValid() const
	{
		return !ResourceId.IsNone() && Count > 0;
	}
};

/**
 * Player-owned replicated data that should survive pawn replacement.
 */
UCLASS()
class PROJECTA_API APRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APRPlayerState();

	virtual void BeginPlay() override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAbilitySystemComponent* GetProjectRiftAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "GAS")
	UPRAttributeSet* GetAttributeSet() const { return AttributeSet.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UPRInventoryComponent* GetInventoryComponent() const { return InventoryComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsReady() const { return bIsReady; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetReady(bool bReady);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FName GetSelectedRoleModule() const { return SelectedRoleModule; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetSelectedRoleModule(FName InSelectedRoleModule);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FString GetPlayerDisplayName() const { return PlayerDisplayName; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby")
	void SetPlayerDisplayName(const FString& InPlayerDisplayName);

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	TArray<FPRShipResourceStack> GetShipResources() const { return ShipResources; }

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	int32 GetShipResourceCount(FName ResourceId) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ship|Resources")
	bool AddShipResource(FName ResourceId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ship|Resources")
	void ResetShipResources();

	UFUNCTION(BlueprintPure, Category = "Ship|Resources")
	FString BuildShipResourceSummary() const;

private:
	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_PlayerDisplayName();

	UFUNCTION()
	void OnRep_ShipResources();

	int32 FindShipResourceIndex(FName ResourceId) const;
	void CopyProjectRiftStateFrom(const APRPlayerState* SourcePlayerState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPRInventoryComponent> InventoryComponent;

	UPROPERTY(ReplicatedUsing = OnRep_IsReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsReady;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FName SelectedRoleModule;

	UPROPERTY(ReplicatedUsing = OnRep_PlayerDisplayName, VisibleInstanceOnly, BlueprintReadOnly, Category = "Lobby", meta = (AllowPrivateAccess = "true"))
	FString PlayerDisplayName;

	UPROPERTY(ReplicatedUsing = OnRep_ShipResources, VisibleInstanceOnly, BlueprintReadOnly, Category = "Ship|Resources", meta = (AllowPrivateAccess = "true"))
	TArray<FPRShipResourceStack> ShipResources;
};
