#include "Production/PRProductionLevelValidator.h"

#include "Bosses/PRBossEncounterController.h"
#include "Core/PREnemySpawnPoint.h"
#include "Core/PRExtractionZone.h"
#include "Core/PRProductionLevelMarker.h"
#include "Core/PRRiftObjectiveActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace
{
void AddLevelIssue(TArray<FPRProductionValidationIssue>& Issues, const TCHAR* RuleId, const FString& MapPath, const FString& Message, const FString& Remediation)
{
	FPRProductionValidationIssue& Issue = Issues.AddDefaulted_GetRef();
	Issue.RuleId = RuleId;
	Issue.Severity = EPRProductionValidationSeverity::Error;
	Issue.AssetPath = MapPath;
	Issue.Message = Message;
	Issue.Remediation = Remediation;
}

UWorld* LoadMapWorld(const FString& MapPath)
{
	const FString ObjectPath = MapPath + TEXT(".") + FPackageName::GetShortName(MapPath);
	return LoadObject<UWorld>(nullptr, *ObjectPath);
}
}

void FPRProductionLevelValidator::Validate(const FPRProductionPolicy& Policy, TArray<FPRProductionValidationIssue>& OutIssues)
{
	for (const FPRProductionLevelContract& Contract : Policy.LevelContracts)
	{
		UWorld* World = LoadMapWorld(Contract.MapPath);
		if (!World || !World->PersistentLevel)
		{
			AddLevelIssue(OutIssues, TEXT("PRP-LVL-000"), Contract.MapPath, TEXT("Declared production map could not be loaded."), TEXT("Create or correct the declared production map package."));
			continue;
		}

		TSet<FName> SpawnGroups;
		TSet<FName> ObjectiveNodes;
		TSet<FName> StreamingPartitions;
		bool bHasExtraction = false;
		bool bHasBossArena = false;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (const APREnemySpawnPoint* SpawnPoint = Cast<APREnemySpawnPoint>(Actor))
			{
				if (!SpawnPoint->GetSpawnGroupId().IsNone())
				{
					SpawnGroups.Add(SpawnPoint->GetSpawnGroupId());
				}
			}
			if (const APRRiftObjectiveActor* Objective = Cast<APRRiftObjectiveActor>(Actor))
			{
				if (!Objective->GetObjectiveNodeId().IsNone())
				{
					ObjectiveNodes.Add(Objective->GetObjectiveNodeId());
				}
			}
			bHasExtraction |= Actor && Actor->IsA<APRExtractionZone>();
			bHasBossArena |= Actor && Actor->IsA<APRBossEncounterController>();
			if (const APRProductionLevelMarker* Marker = Cast<APRProductionLevelMarker>(Actor))
			{
				if (Marker->GetMarkerKind() == EPRProductionLevelMarkerKind::StreamingPartition && !Marker->GetMarkerId().IsNone())
				{
					StreamingPartitions.Add(Marker->GetMarkerId());
				}
			}
		}

		for (const FName RequiredId : Contract.RequiredSpawnGroupIds)
		{
			if (!SpawnGroups.Contains(RequiredId))
			{
				AddLevelIssue(OutIssues, TEXT("PRP-LVL-001"), Contract.MapPath, FString::Printf(TEXT("Required SpawnGroup '%s' is missing."), *RequiredId.ToString()), TEXT("Place an enemy spawn point with the declared SpawnGroupId."));
			}
		}
		for (const FName RequiredId : Contract.RequiredObjectiveNodeIds)
		{
			if (!ObjectiveNodes.Contains(RequiredId))
			{
				AddLevelIssue(OutIssues, TEXT("PRP-LVL-002"), Contract.MapPath, FString::Printf(TEXT("Required ObjectiveSocket '%s' is missing."), *RequiredId.ToString()), TEXT("Place a graph-linked ProjectRift objective actor with the declared ObjectiveNodeId."));
			}
		}
		if (Contract.bRequiresExtractionSocket && !bHasExtraction)
		{
			AddLevelIssue(OutIssues, TEXT("PRP-LVL-003"), Contract.MapPath, TEXT("Required ExtractionSocket is missing."), TEXT("Place an APRExtractionZone actor."));
		}
		if (Contract.bRequiresBossArena && !bHasBossArena)
		{
			AddLevelIssue(OutIssues, TEXT("PRP-LVL-004"), Contract.MapPath, TEXT("Required BossArena is missing."), TEXT("Place an APRBossEncounterController actor."));
		}
		for (const FName RequiredId : Contract.RequiredStreamingPartitionIds)
		{
			if (!StreamingPartitions.Contains(RequiredId))
			{
				AddLevelIssue(OutIssues, TEXT("PRP-LVL-005"), Contract.MapPath, FString::Printf(TEXT("Required StreamingPartition '%s' is missing."), *RequiredId.ToString()), TEXT("Place an APRProductionLevelMarker with the declared stable ID."));
			}
		}
	}
}
