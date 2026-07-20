#include "Crafting/PRCraftingRecipeDataAsset.h"

namespace ProjectRiftCraftingPrivate
{
bool AreUniqueValidNames(const TArray<FName>& Names)
{
	TSet<FName> Seen;
	for (const FName Name : Names)
	{
		if (Name.IsNone() || Seen.Contains(Name))
		{
			return false;
		}
		Seen.Add(Name);
	}
	return true;
}
}

const FPrimaryAssetType UPRCraftingRecipeDataAsset::CraftingRecipePrimaryAssetType(TEXT("ProjectRiftRecipe"));

FPrimaryAssetId UPRCraftingRecipeDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(CraftingRecipePrimaryAssetType, RecipeId.IsNone() ? GetFName() : RecipeId);
}

bool UPRCraftingRecipeDataAsset::IsRecipeValid() const
{
	FString Diagnostic;
	return ValidateRecipe(Diagnostic);
}

bool UPRCraftingRecipeDataAsset::ValidateRecipe(FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
	if (RecipeId.IsNone() || DisplayName.IsEmpty() || OutputItemId.IsNone() || OutputCount <= 0
		|| EquipmentAffixCount < 0 || EquipmentAffixCount > 3)
	{
		OutDiagnostic = TEXT("Recipe id, display name, output item, positive output count, and valid affix count are required.");
		return false;
	}
	if (ResourceCosts.IsEmpty())
	{
		OutDiagnostic = TEXT("Crafting recipe requires at least one resource cost.");
		return false;
	}
	TSet<FName> ResourceIds;
	for (const FPRCraftingResourceCost& Cost : ResourceCosts)
	{
		if (Cost.ResourceId.IsNone() || Cost.Count <= 0 || ResourceIds.Contains(Cost.ResourceId))
		{
			OutDiagnostic = TEXT("Crafting recipe contains an invalid or duplicate resource cost.");
			return false;
		}
		ResourceIds.Add(Cost.ResourceId);
	}
	if (!ProjectRiftCraftingPrivate::AreUniqueValidNames(RequiredCompletedStoryNodeIds))
	{
		OutDiagnostic = TEXT("Crafting story prerequisites contain an invalid or duplicate id.");
		return false;
	}
	if ((!RequiredShipModuleId.IsNone() && RequiredShipModuleLevel < 1)
		|| (RequiredShipModuleId.IsNone() && RequiredShipModuleLevel != 0))
	{
		OutDiagnostic = TEXT("Crafting ship module id and level must be specified together.");
		return false;
	}
	return true;
}

bool UPRCraftingRecipeDataAsset::ValidateCatalog(
	const TArray<UPRCraftingRecipeDataAsset*>& Recipes,
	FString& OutDiagnostic)
{
	TSet<FName> RecipeIds;
	for (const UPRCraftingRecipeDataAsset* Recipe : Recipes)
	{
		FString Diagnostic;
		if (!Recipe || !Recipe->ValidateRecipe(Diagnostic) || RecipeIds.Contains(Recipe->RecipeId))
		{
			OutDiagnostic = Diagnostic.IsEmpty() ? TEXT("Crafting recipe catalog contains a null or duplicate recipe.") : Diagnostic;
			return false;
		}
		RecipeIds.Add(Recipe->RecipeId);
	}
	OutDiagnostic.Reset();
	return true;
}

FPRCraftingEvaluation UPRCraftingRecipeDataAsset::EvaluateRecipe(const FPRProfileSnapshot& Snapshot) const
{
	FPRCraftingEvaluation Evaluation;
	FString Diagnostic;
	if (!ValidateRecipe(Diagnostic))
	{
		Evaluation.Diagnostic = Diagnostic;
		return Evaluation;
	}
	for (const FName StoryNodeId : RequiredCompletedStoryNodeIds)
	{
		if (!Snapshot.Story.CompletedStoryNodeIds.Contains(StoryNodeId))
		{
			Evaluation.Availability = EPRCraftingAvailability::MissingStoryProgress;
			Evaluation.Diagnostic = FString::Printf(TEXT("Required story node %s is incomplete."), *StoryNodeId.ToString());
			return Evaluation;
		}
	}
	if (!RequiredChapterId.IsNone() && !Snapshot.Story.UnlockedChapterIds.Contains(RequiredChapterId))
	{
		Evaluation.Availability = EPRCraftingAvailability::MissingChapter;
		Evaluation.Diagnostic = FString::Printf(TEXT("Required chapter %s is locked."), *RequiredChapterId.ToString());
		return Evaluation;
	}
	if (!RequiredShipModuleId.IsNone())
	{
		const FPRProfileShipModuleState* Module = Snapshot.ShipModules.FindByPredicate([this](const FPRProfileShipModuleState& Candidate)
		{
			return Candidate.ModuleId == RequiredShipModuleId;
		});
		if (!Module || !Module->bUnlocked || Module->Level < RequiredShipModuleLevel)
		{
			Evaluation.Availability = EPRCraftingAvailability::MissingShipModule;
			Evaluation.Diagnostic = FString::Printf(TEXT("Required ship module %s is unavailable."), *RequiredShipModuleId.ToString());
			return Evaluation;
		}
	}
	for (const FPRCraftingResourceCost& Cost : ResourceCosts)
	{
		if (Snapshot.GetResourceCount(Cost.ResourceId) < Cost.Count)
		{
			Evaluation.Availability = EPRCraftingAvailability::InsufficientResources;
			Evaluation.Diagnostic = FString::Printf(TEXT("Insufficient %s."), *Cost.ResourceId.ToString());
			return Evaluation;
		}
	}
	Evaluation.Availability = EPRCraftingAvailability::Available;
	Evaluation.Diagnostic = TEXT("Crafting recipe is available.");
	return Evaluation;
}

