#include "Progression/PRMissionProgressionDataAsset.h"

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

	return true;
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
