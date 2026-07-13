#include "Ship/PRShipRepairDataAsset.h"

namespace ProjectRiftShipRepairPrivate
{
template <typename PredicateType>
bool ValidateUniqueNames(const TArray<FName>& Names, PredicateType&& Predicate)
{
	TSet<FName> Seen;
	for (const FName Name : Names)
	{
		if (Name.IsNone() || Seen.Contains(Name) || !Predicate(Name))
		{
			return false;
		}
		Seen.Add(Name);
	}
	return true;
}

const FPRProfileShipModuleState* FindModule(
	const FPRProfileSnapshot& Snapshot,
	const FName ModuleId)
{
	return Snapshot.ShipModules.FindByPredicate([ModuleId](const FPRProfileShipModuleState& Module)
	{
		return Module.ModuleId == ModuleId;
	});
}

bool IsRepairCompleted(
	const UPRShipRepairDataAsset* Contract,
	const FPRProfileSnapshot& Snapshot)
{
	const FPRProfileShipModuleState* Module = Contract ? FindModule(Snapshot, Contract->ModuleId) : nullptr;
	return Module && Module->bUnlocked && Module->Level >= Contract->TargetLevel;
}
}

const FPrimaryAssetType UPRShipRepairDataAsset::ShipRepairPrimaryAssetType(TEXT("ProjectRiftShipRepair"));

FPrimaryAssetId UPRShipRepairDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ShipRepairPrimaryAssetType, RepairProjectId.IsNone() ? GetFName() : RepairProjectId);
}

bool UPRShipRepairDataAsset::IsContractValid() const
{
	return ValidateContract(nullptr);
}

bool UPRShipRepairDataAsset::ValidateContract(FString* OutDiagnostic) const
{
	if (RepairProjectId.IsNone() || ModuleId.IsNone() || TargetLevel < 1 || DisplayName.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair project id, display name, module id, and positive target level are required."); }
		return false;
	}
	if (ResourceCosts.IsEmpty())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair contract requires at least one resource cost."); }
		return false;
	}
	TSet<FName> ResourceIds;
	for (const FPRShipRepairResourceCost& Cost : ResourceCosts)
	{
		if (Cost.ResourceId.IsNone() || Cost.Count <= 0 || ResourceIds.Contains(Cost.ResourceId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair resource costs contain an invalid or duplicate entry."); }
			return false;
		}
		ResourceIds.Add(Cost.ResourceId);
	}
	if (!ProjectRiftShipRepairPrivate::ValidateUniqueNames(
		RequiredRepairProjectIds,
		[this](const FName RequiredId) { return RequiredId != RepairProjectId; }))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair prerequisites contain an invalid, duplicate, or self-referencing id."); }
		return false;
	}
	if (!ProjectRiftShipRepairPrivate::ValidateUniqueNames(
		RequiredCompletedStoryNodeIds,
		[](const FName) { return true; }))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Story prerequisites contain an invalid or duplicate id."); }
		return false;
	}
	if (!ProjectRiftShipRepairPrivate::ValidateUniqueNames(
		UnlockedChapterIdsOnCompletion,
		[](const FName) { return true; }))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Unlocked chapters contain an invalid or duplicate id."); }
		return false;
	}
	if (!NextChapterId.IsNone() && !UnlockedChapterIdsOnCompletion.Contains(NextChapterId))
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Next chapter must be included in the unlocked chapter results."); }
		return false;
	}
	return true;
}

bool UPRShipRepairDataAsset::ValidateCatalog(
	const TArray<UPRShipRepairDataAsset*>& Contracts,
	FString* OutDiagnostic)
{
	TMap<FName, const UPRShipRepairDataAsset*> ContractById;
	for (const UPRShipRepairDataAsset* Contract : Contracts)
	{
		FString Diagnostic;
		if (!Contract || !Contract->ValidateContract(&Diagnostic) || ContractById.Contains(Contract->RepairProjectId))
		{
			if (OutDiagnostic)
			{
				*OutDiagnostic = Diagnostic.IsEmpty() ? TEXT("Repair catalog contains a null or duplicate contract.") : Diagnostic;
			}
			return false;
		}
		ContractById.Add(Contract->RepairProjectId, Contract);
	}
	for (const TPair<FName, const UPRShipRepairDataAsset*>& Pair : ContractById)
	{
		for (const FName RequiredId : Pair.Value->RequiredRepairProjectIds)
		{
			if (!ContractById.Contains(RequiredId))
			{
				if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("Repair prerequisite %s is not registered."), *RequiredId.ToString()); }
				return false;
			}
		}
	}

	TMap<FName, uint8> VisitState;
	TFunction<bool(FName)> Visit = [&](const FName ProjectId)
	{
		uint8& State = VisitState.FindOrAdd(ProjectId);
		if (State == 1)
		{
			return false;
		}
		if (State == 2)
		{
			return true;
		}
		State = 1;
		for (const FName RequiredId : ContractById.FindChecked(ProjectId)->RequiredRepairProjectIds)
		{
			if (!Visit(RequiredId))
			{
				return false;
			}
		}
		State = 2;
		return true;
	};
	for (const TPair<FName, const UPRShipRepairDataAsset*>& Pair : ContractById)
	{
		if (!Visit(Pair.Key))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Repair prerequisite graph contains a cycle."); }
			return false;
		}
	}
	return true;
}

FPRShipRepairEvaluation UPRShipRepairDataAsset::EvaluateRepair(
	const FPRProfileSnapshot& Snapshot,
	const TArray<UPRShipRepairDataAsset*>& Catalog) const
{
	FPRShipRepairEvaluation Evaluation;
	FString Diagnostic;
	if (!ValidateCatalog(Catalog, &Diagnostic)
		|| !Catalog.ContainsByPredicate([this](const UPRShipRepairDataAsset* Contract)
		{
			return Contract && Contract->RepairProjectId == RepairProjectId;
		}))
	{
		Evaluation.Availability = EPRShipRepairAvailability::InvalidContract;
		Evaluation.Diagnostic = Diagnostic.IsEmpty() ? TEXT("Repair contract is not registered in the catalog.") : Diagnostic;
		return Evaluation;
	}
	if (ProjectRiftShipRepairPrivate::IsRepairCompleted(this, Snapshot))
	{
		Evaluation.Availability = EPRShipRepairAvailability::AlreadyCompleted;
		Evaluation.Diagnostic = TEXT("Repair project is already completed.");
		return Evaluation;
	}
	for (const FName RequiredId : RequiredRepairProjectIds)
	{
		const UPRShipRepairDataAsset* const* RequiredContract = Catalog.FindByPredicate([RequiredId](const UPRShipRepairDataAsset* Contract)
		{
			return Contract && Contract->RepairProjectId == RequiredId;
		});
		if (!RequiredContract || !ProjectRiftShipRepairPrivate::IsRepairCompleted(*RequiredContract, Snapshot))
		{
			Evaluation.Availability = EPRShipRepairAvailability::MissingPrerequisiteRepair;
			Evaluation.Diagnostic = FString::Printf(TEXT("Required repair %s is incomplete."), *RequiredId.ToString());
			return Evaluation;
		}
	}
	for (const FName RequiredStoryId : RequiredCompletedStoryNodeIds)
	{
		if (!Snapshot.Story.CompletedStoryNodeIds.Contains(RequiredStoryId))
		{
			Evaluation.Availability = EPRShipRepairAvailability::MissingStoryProgress;
			Evaluation.Diagnostic = FString::Printf(TEXT("Required story node %s is incomplete."), *RequiredStoryId.ToString());
			return Evaluation;
		}
	}
	for (const FPRShipRepairResourceCost& Cost : ResourceCosts)
	{
		if (Snapshot.GetResourceCount(Cost.ResourceId) < Cost.Count)
		{
			Evaluation.Availability = EPRShipRepairAvailability::InsufficientResources;
			Evaluation.Diagnostic = FString::Printf(TEXT("Insufficient %s."), *Cost.ResourceId.ToString());
			return Evaluation;
		}
	}
	Evaluation.Availability = EPRShipRepairAvailability::Available;
	Evaluation.Diagnostic = TEXT("Repair is available.");
	return Evaluation;
}

bool UPRShipRepairDataAsset::ApplyRepairToSnapshot(
	FPRProfileSnapshot& Snapshot,
	const TArray<UPRShipRepairDataAsset*>& Catalog,
	FString* OutDiagnostic) const
{
	const FPRShipRepairEvaluation Evaluation = EvaluateRepair(Snapshot, Catalog);
	if (!Evaluation.IsAvailable())
	{
		if (OutDiagnostic) { *OutDiagnostic = Evaluation.Diagnostic; }
		return false;
	}
	FPRProfileSnapshot Candidate = Snapshot;
	for (const FPRShipRepairResourceCost& Cost : ResourceCosts)
	{
		FPRProfileResourceBalance* Balance = Candidate.ResourceWallet.FindByPredicate([&Cost](const FPRProfileResourceBalance& Entry)
		{
			return Entry.ResourceId == Cost.ResourceId;
		});
		check(Balance);
		Balance->Count -= Cost.Count;
	}
	FPRProfileShipModuleState* Module = Candidate.ShipModules.FindByPredicate([this](const FPRProfileShipModuleState& Entry)
	{
		return Entry.ModuleId == ModuleId;
	});
	if (!Module)
	{
		Candidate.ShipModules.Emplace(ModuleId, TargetLevel, true);
	}
	else
	{
		Module->Level = FMath::Max(Module->Level, TargetLevel);
		Module->bUnlocked = true;
	}
	for (const FName ChapterId : UnlockedChapterIdsOnCompletion)
	{
		Candidate.Story.UnlockedChapterIds.AddUnique(ChapterId);
	}
	if (!NextChapterId.IsNone())
	{
		Candidate.Story.UnlockedChapterIds.AddUnique(NextChapterId);
		Candidate.Story.CurrentChapterId = NextChapterId;
	}
	Candidate.Normalize();
	FString Diagnostic;
	if (!Candidate.IsValid(&Diagnostic))
	{
		if (OutDiagnostic) { *OutDiagnostic = Diagnostic; }
		return false;
	}
	Snapshot = MoveTemp(Candidate);
	return true;
}
