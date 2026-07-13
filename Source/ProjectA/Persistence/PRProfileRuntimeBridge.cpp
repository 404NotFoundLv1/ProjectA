#include "Persistence/PRProfileRuntimeBridge.h"

#include "Items/PRInventoryComponent.h"
#include "Player/PRPlayerState.h"

bool FPRProfileRuntimeBridge::CaptureFromPlayerState(
	const APRPlayerState* PlayerState,
	FPRProfileSnapshot& InOutSnapshot,
	FString& OutDiagnostic)
{
	const UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!PlayerState || !Inventory)
	{
		OutDiagnostic = TEXT("PlayerState or inventory component is unavailable.");
		return false;
	}
	InOutSnapshot.BackpackItems = Inventory->GetInventoryItems();
	InOutSnapshot.ResourceWallet.Reset();
	for (const FPRShipResourceStack& Resource : PlayerState->GetShipResources())
	{
		if (Resource.IsValid())
		{
			InOutSnapshot.ResourceWallet.Emplace(Resource.ResourceId, Resource.Count);
		}
	}
	InOutSnapshot.SelectedRoleId = PlayerState->GetSelectedRoleModule();
	InOutSnapshot.Normalize();
	return InOutSnapshot.IsValid(&OutDiagnostic);
}

bool FPRProfileRuntimeBridge::ApplyToPlayerState(
	const FPRProfileSnapshot& Snapshot,
	APRPlayerState* PlayerState,
	FString& OutDiagnostic)
{
	UPRInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!PlayerState || !Inventory)
	{
		OutDiagnostic = TEXT("PlayerState or inventory component is unavailable.");
		return false;
	}
	if (!Snapshot.IsValid(&OutDiagnostic))
	{
		return false;
	}
	TArray<FPRShipResourceStack> Resources;
	Resources.Reserve(Snapshot.ResourceWallet.Num());
	for (const FPRProfileResourceBalance& Balance : Snapshot.ResourceWallet)
	{
		FPRShipResourceStack& Resource = Resources.AddDefaulted_GetRef();
		Resource.ResourceId = Balance.ResourceId;
		Resource.Count = Balance.Count;
	}
	if (!Inventory->ReplaceInventoryItems(Snapshot.BackpackItems)
		|| !PlayerState->ReplaceShipResources(Resources))
	{
		OutDiagnostic = TEXT("PlayerState rejected validated profile data.");
		return false;
	}
	PlayerState->SetSelectedRoleModule(Snapshot.SelectedRoleId);
	return true;
}
