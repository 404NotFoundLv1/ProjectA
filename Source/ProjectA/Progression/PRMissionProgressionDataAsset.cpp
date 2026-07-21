#include "Progression/PRMissionProgressionDataAsset.h"

#include "Misc/Crc.h"

const FPrimaryAssetType UPRMissionProgressionDataAsset::MissionPrimaryAssetType(TEXT("ProjectRiftMission"));

FPrimaryAssetId UPRMissionProgressionDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(MissionPrimaryAssetType, MissionId.IsNone() ? GetFName() : MissionId);
}

bool UPRMissionProgressionDataAsset::IsContractValid(FString* OutDiagnostic) const
{
	if (MissionId.IsNone() || MissionMap.IsNull() || ChapterId.IsNone() || CompletionStoryNodeId.IsNone())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission id, map, chapter, and completion node id are required."); }
		return false;
	}

	TSet<FName> RequiredIds;
	for (const FName RequiredId : RequiredCompletedStoryNodeIds)
	{
		if (RequiredId.IsNone() || RequiredIds.Contains(RequiredId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission prerequisites contain an invalid or duplicate id."); }
			return false;
		}
		RequiredIds.Add(RequiredId);
	}

	TSet<FName> UnlockIds;
	for (const FName UnlockId : UnlockedChapterIdsOnCompletion)
	{
		if (UnlockId.IsNone() || UnlockIds.Contains(UnlockId))
		{
			if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission chapter unlocks contain an invalid or duplicate id."); }
			return false;
		}
		UnlockIds.Add(UnlockId);
	}

	return ValidateMissionContract(OutDiagnostic) && ValidateObjectiveGraph(OutDiagnostic);
}

bool UPRMissionProgressionDataAsset::ValidateMissionContract(FString* OutDiagnostic) const
{
	FPRMissionContract EffectiveContract = Contract;
	// Existing authored mission assets predate the v0.8.0 struct.  Serialized
	// zero values mean "use the test-contract defaults", not an invalid live
	// mission; partial authored values still use normal fail-closed validation.
	if (EffectiveContract.ContractVersion == 0 && EffectiveContract.BiomeId.IsNone()
		&& EffectiveContract.DifficultyId.IsNone())
	{
		EffectiveContract = FPRMissionContract();
	}
	return EffectiveContract.IsValid(OutDiagnostic);
}

bool UPRMissionProgressionDataAsset::ValidateObjectiveGraph(FString* OutDiagnostic) const
{
	return !HasObjectiveGraph() || ObjectiveGraph.IsValid(OutDiagnostic);
}

FPRMissionDefinition UPRMissionProgressionDataAsset::BuildMissionDefinition(const int32 Seed, FString* OutDiagnostic) const
{
	FPRMissionDefinition Definition;
	if (Seed == 0)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission definition requires a non-zero seed."); }
		return Definition;
	}
	if (!IsContractValid(OutDiagnostic))
	{
		return Definition;
	}

	Definition.ContractId = MissionId;
	FPRMissionContract EffectiveContract = Contract;
	if (EffectiveContract.ContractVersion == 0 && EffectiveContract.BiomeId.IsNone() && EffectiveContract.DifficultyId.IsNone()) { EffectiveContract = FPRMissionContract(); }
	Definition.ContractVersion = EffectiveContract.ContractVersion;
	Definition.BiomeId = EffectiveContract.BiomeId;
	Definition.DifficultyId = EffectiveContract.DifficultyId;
	Definition.ModifierIds = EffectiveContract.ModifierIds;
	Definition.ModifierIds.Sort(FNameLexicalLess());
	Definition.RewardPreview = EffectiveContract.RewardPreview;
	Definition.Seed = Seed;

	FString SignatureSource = FString::Printf(TEXT("%s|%d|%s|%s|%d|%d|%d|%d|%d"),
		*Definition.ContractId.ToString(), Definition.ContractVersion, *Definition.BiomeId.ToString(), *Definition.DifficultyId.ToString(), Definition.Seed,
		Definition.RewardPreview.MinimumBudget, Definition.RewardPreview.MaximumBudget, Definition.RewardPreview.MinimumRarity, Definition.RewardPreview.MaximumRarity);
	for (const FName ModifierId : Definition.ModifierIds)
	{
		SignatureSource += TEXT("|") + ModifierId.ToString();
	}
	for (const FName RewardTypeId : Definition.RewardPreview.RewardTypeIds)
	{
		SignatureSource += TEXT("|") + RewardTypeId.ToString();
	}
	if (HasObjectiveGraph())
	{
		SignatureSource += FString::Printf(TEXT("|Graph:%d"), ObjectiveGraph.ComputeSignature());
	}
	Definition.DeterministicSignature = static_cast<int32>(FCrc::StrCrc32(*SignatureSource));
	return Definition;
}

bool UPRMissionProgressionDataAsset::IsEligible(const FPRProfileStoryProgress& Story) const
{
	if (!IsContractValid())
	{
		return false;
	}
	if (!bStarterMission && !Story.UnlockedChapterIds.Contains(ChapterId))
	{
		return false;
	}
	return !RequiredCompletedStoryNodeIds.ContainsByPredicate([&Story](const FName RequiredId)
	{
		return !Story.CompletedStoryNodeIds.Contains(RequiredId);
	});
}

bool UPRMissionProgressionDataAsset::ApplyCompletion(FPRProfileStoryProgress& Story) const
{
	if (!IsEligible(Story) || Story.CompletedStoryNodeIds.Contains(CompletionStoryNodeId))
	{
		return false;
	}

	Story.CompletedStoryNodeIds.Add(CompletionStoryNodeId);
	Story.UnlockedChapterIds.AddUnique(ChapterId);
	for (const FName UnlockId : UnlockedChapterIdsOnCompletion)
	{
		Story.UnlockedChapterIds.AddUnique(UnlockId);
	}
	Story.CurrentChapterId = NextChapterId.IsNone() ? ChapterId : NextChapterId;
	Story.UnlockedChapterIds.AddUnique(Story.CurrentChapterId);
	return true;
}
