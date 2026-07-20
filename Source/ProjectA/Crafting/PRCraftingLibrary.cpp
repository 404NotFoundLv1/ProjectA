#include "Crafting/PRCraftingLibrary.h"

#include "Crafting/PRCraftingRecipeDataAsset.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PRAffixGenerationLibrary.h"
#include "Items/PREquipmentDataAsset.h"
#include "Items/PRItemDataAsset.h"

namespace ProjectRiftCraftingLibraryPrivate
{
bool AddItemToSnapshot(
	TArray<FPRItemInstance>& Items,
	const FPRItemInstance& Incoming,
	const int32 MaxStackCount,
	FPRItemInstance& OutFirstCreated)
{
	if (!Incoming.IsValid() || MaxStackCount <= 0)
	{
		return false;
	}
	int32 Remaining = Incoming.Count;
	for (FPRItemInstance& Existing : Items)
	{
		if (Remaining <= 0 || !Existing.HasEquivalentStackingState(Incoming) || Existing.Count >= MaxStackCount)
		{
			continue;
		}
		const int32 Added = FMath::Min(Remaining, MaxStackCount - Existing.Count);
		Existing.Count += Added;
		Remaining -= Added;
		if (!OutFirstCreated.IsValid())
		{
			OutFirstCreated = Existing;
		}
	}
	while (Remaining > 0)
	{
		FPRItemInstance NewItem = Incoming;
		NewItem.InstanceGuid = FGuid::NewGuid();
		NewItem.Count = FMath::Min(Remaining, MaxStackCount);
		Items.Add(NewItem);
		Remaining -= NewItem.Count;
		if (!OutFirstCreated.IsValid())
		{
			OutFirstCreated = NewItem;
		}
	}
	return true;
}

bool ApplyCosts(FPRProfileSnapshot& Snapshot, const TArray<FPRCraftingResourceCost>& Costs)
{
	for (const FPRCraftingResourceCost& Cost : Costs)
	{
		FPRProfileResourceBalance* Balance = Snapshot.ResourceWallet.FindByPredicate([&Cost](const FPRProfileResourceBalance& Entry) { return Entry.ResourceId == Cost.ResourceId; });
		if (!Balance || Balance->Count < Cost.Count) { return false; }
	}
	for (const FPRCraftingResourceCost& Cost : Costs)
	{
		Snapshot.ResourceWallet.FindByPredicate([&Cost](const FPRProfileResourceBalance& Entry) { return Entry.ResourceId == Cost.ResourceId; })->Count -= Cost.Count;
	}
	return true;
}

void AddResources(FPRProfileSnapshot& Snapshot, const TArray<FPRCraftingResourceCost>& Resources)
{
	for (const FPRCraftingResourceCost& Resource : Resources)
	{
		if (FPRProfileResourceBalance* Balance = Snapshot.ResourceWallet.FindByPredicate([&Resource](const FPRProfileResourceBalance& Entry) { return Entry.ResourceId == Resource.ResourceId; }))
		{
			Balance->Count += Resource.Count;
		}
		else { Snapshot.ResourceWallet.Add({ Resource.ResourceId, Resource.Count }); }
	}
}
}

bool UPRCraftingLibrary::ApplyRecipeToSnapshot(
	const UPRCraftingRecipeDataAsset* Recipe,
	const UPRItemDataAsset* OutputDefinition,
	const int32 CraftSeed,
	const TArray<UPRAffixDefinitionDataAsset*>& AffixCatalog,
	FPRProfileSnapshot& InOutSnapshot,
	FPRItemInstance& OutCraftedItem,
	FPRCraftingEvaluation& OutEvaluation)
{
	OutCraftedItem = FPRItemInstance();
	OutEvaluation = Recipe ? Recipe->EvaluateRecipe(InOutSnapshot) : FPRCraftingEvaluation();
	if (!OutEvaluation.IsAvailable())
	{
		return false;
	}
	if (!OutputDefinition || !OutputDefinition->bCanCraft || OutputDefinition->ItemId != Recipe->OutputItemId)
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InvalidRecipe;
		OutEvaluation.Diagnostic = TEXT("Crafting output definition is unavailable or cannot be crafted.");
		return false;
	}

	FPRProfileSnapshot Candidate = InOutSnapshot;
	for (const FPRCraftingResourceCost& Cost : Recipe->ResourceCosts)
	{
		FPRProfileResourceBalance* Balance = Candidate.ResourceWallet.FindByPredicate([&Cost](const FPRProfileResourceBalance& Entry)
		{
			return Entry.ResourceId == Cost.ResourceId;
		});
		if (!Balance || Balance->Count < Cost.Count)
		{
			OutEvaluation.Availability = EPRCraftingAvailability::InsufficientResources;
			OutEvaluation.Diagnostic = TEXT("Authoritative resource balance changed before crafting could commit.");
			return false;
		}
		Balance->Count -= Cost.Count;
	}

	FPRItemInstance CraftedItem;
	if (const UPREquipmentDataAsset* EquipmentDefinition = Cast<UPREquipmentDataAsset>(OutputDefinition))
	{
		const FPRAffixGenerationResult Generated = UPRAffixGenerationLibrary::GenerateEquipmentInstanceWithFixedRarity(
			EquipmentDefinition, CraftSeed, AffixCatalog, Recipe->EquipmentRarity, Recipe->EquipmentAffixCount);
		if (!Generated.bSuccess)
		{
			OutEvaluation.Availability = EPRCraftingAvailability::InvalidRecipe;
			OutEvaluation.Diagnostic = Generated.Diagnostic;
			return false;
		}
		CraftedItem = Generated.Item;
		CraftedItem.Count = Recipe->OutputCount;
	}
	else
	{
		CraftedItem.ItemId = Recipe->OutputItemId;
		CraftedItem.Count = Recipe->OutputCount;
		CraftedItem.Level = 1;
		CraftedItem.Rarity = Recipe->EquipmentRarity;
		CraftedItem.Durability = 1.0f;
	}
	FPRItemInstance FirstCreated;
	if (!ProjectRiftCraftingLibraryPrivate::AddItemToSnapshot(
		Candidate.BackpackItems,
		CraftedItem,
		FMath::Max(1, OutputDefinition->MaxStackCount),
		FirstCreated))
	{
		OutEvaluation.Availability = EPRCraftingAvailability::CapacityExceeded;
		OutEvaluation.Diagnostic = TEXT("Crafted output cannot be added to the profile backpack.");
		return false;
	}
	Candidate.Normalize();
	FString Diagnostic;
	if (!Candidate.IsValid(&Diagnostic))
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InvalidRecipe;
		OutEvaluation.Diagnostic = Diagnostic;
		return false;
	}

	InOutSnapshot = MoveTemp(Candidate);
	OutCraftedItem = FirstCreated;
	OutEvaluation.Availability = EPRCraftingAvailability::Available;
	OutEvaluation.Diagnostic = TEXT("Crafting recipe committed to the candidate profile state.");
	return true;
}

bool UPRCraftingLibrary::ApplyDismantleToSnapshot(const FGuid& InstanceGuid, const UPRItemDataAsset* ItemDefinition, FPRProfileSnapshot& InOutSnapshot, FPRCraftingEvaluation& OutEvaluation)
{
	OutEvaluation = FPRCraftingEvaluation();
	if (!InstanceGuid.IsValid() || !ItemDefinition || !ItemDefinition->bCanDismantle || ItemDefinition->DismantleResult.ResourceReturns.IsEmpty())
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget;
		OutEvaluation.Diagnostic = TEXT("This item cannot be dismantled.");
		return false;
	}
	FPRProfileSnapshot Candidate = InOutSnapshot;
	const int32 Index = Candidate.BackpackItems.IndexOfByPredicate([&InstanceGuid, ItemDefinition](const FPRItemInstance& Item) { return Item.InstanceGuid == InstanceGuid && Item.ItemId == ItemDefinition->ItemId; });
	if (Index == INDEX_NONE)
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget;
		OutEvaluation.Diagnostic = TEXT("Only an unequipped backpack instance may be dismantled.");
		return false;
	}
	Candidate.BackpackItems.RemoveAt(Index);
	ProjectRiftCraftingLibraryPrivate::AddResources(Candidate, ItemDefinition->DismantleResult.ResourceReturns);
	Candidate.Normalize();
	FString Diagnostic;
	if (!Candidate.IsValid(&Diagnostic)) { OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget; OutEvaluation.Diagnostic = Diagnostic; return false; }
	InOutSnapshot = MoveTemp(Candidate);
	OutEvaluation.Availability = EPRCraftingAvailability::Available;
	OutEvaluation.Diagnostic = TEXT("Dismantle committed to the candidate profile state.");
	return true;
}

bool UPRCraftingLibrary::ApplyUpgradeToSnapshot(const FGuid& InstanceGuid, const UPREquipmentDataAsset* EquipmentDefinition, FPRProfileSnapshot& InOutSnapshot, FPRCraftingEvaluation& OutEvaluation)
{
	OutEvaluation = FPRCraftingEvaluation();
	if (!InstanceGuid.IsValid() || !EquipmentDefinition)
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget;
		OutEvaluation.Diagnostic = TEXT("Upgrade target is invalid.");
		return false;
	}
	FPRProfileSnapshot Candidate = InOutSnapshot;
	FPRItemInstance* Target = Candidate.BackpackItems.FindByPredicate([&InstanceGuid, EquipmentDefinition](const FPRItemInstance& Item) { return Item.InstanceGuid == InstanceGuid && Item.ItemId == EquipmentDefinition->ItemId; });
	if (!Target)
	{
		for (FPRProfileEquipmentEntry& Entry : Candidate.Equipment)
		{
			if (Entry.Item.InstanceGuid == InstanceGuid && Entry.Item.ItemId == EquipmentDefinition->ItemId) { Target = &Entry.Item; break; }
		}
	}
	if (!Target) { OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget; OutEvaluation.Diagnostic = TEXT("Upgrade target is not owned by this profile."); return false; }
	if (Target->Level >= EquipmentDefinition->MaxUpgradeLevel)
	{
		OutEvaluation.Availability = EPRCraftingAvailability::AlreadyMaxLevel;
		OutEvaluation.Diagnostic = TEXT("Equipment is already at its maximum upgrade level.");
		return false;
	}
	const int32 NextLevel = Target->Level + 1;
	const FPRUpgradeCost* Cost = EquipmentDefinition->UpgradeCosts.FindByPredicate([NextLevel](const FPRUpgradeCost& Entry) { return Entry.TargetLevel == NextLevel; });
	if (!Cost || Cost->ResourceCosts.IsEmpty() || !ProjectRiftCraftingLibraryPrivate::ApplyCosts(Candidate, Cost->ResourceCosts))
	{
		OutEvaluation.Availability = EPRCraftingAvailability::InsufficientResources;
		OutEvaluation.Diagnostic = TEXT("Upgrade requires unavailable resources or has no authored cost.");
		return false;
	}
	Target->Level = NextLevel;
	Candidate.Normalize();
	FString Diagnostic;
	if (!Candidate.IsValid(&Diagnostic)) { OutEvaluation.Availability = EPRCraftingAvailability::InvalidTarget; OutEvaluation.Diagnostic = Diagnostic; return false; }
	InOutSnapshot = MoveTemp(Candidate);
	OutEvaluation.Availability = EPRCraftingAvailability::Available;
	OutEvaluation.Diagnostic = TEXT("Upgrade committed to the candidate profile state.");
	return true;
}
