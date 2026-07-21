#include "Enemies/PREnemyRosterDataAsset.h"

#include "Enemies/PREnemyCharacter.h"

const FPrimaryAssetType UPREnemyRosterDataAsset::EnemyRosterPrimaryAssetType(TEXT("ProjectRiftEnemyRoster"));

FPrimaryAssetId UPREnemyRosterDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(EnemyRosterPrimaryAssetType, RosterId.IsNone() ? GetFName() : RosterId);
}

bool UPREnemyRosterDataAsset::ValidateRoster(FString& OutDiagnostic) const
{
	if (RosterId.IsNone() || Entries.IsEmpty())
	{
		OutDiagnostic = TEXT("Enemy roster has no identity or entries.");
		return false;
	}

	TSet<FName> SeenEnemyIds;
	for (const FPREnemyRosterEntry& Entry : Entries)
	{
		FString DefinitionDiagnostic;
		if (!Entry.Definition || !Entry.EnemyClass || !Entry.Definition->ValidateDefinition(DefinitionDiagnostic)
			|| !FMath::IsFinite(Entry.SelectionWeight) || Entry.SelectionWeight <= 0.0f
			|| Entry.PackSize <= 0 || Entry.MaxAlive < 0 || Entry.MinimumFrozenPlayerCount <= 0
			|| SeenEnemyIds.Contains(Entry.Definition ? Entry.Definition->EnemyId : NAME_None))
		{
			OutDiagnostic = TEXT("Enemy roster contains an invalid or duplicate entry.");
			return false;
		}
		SeenEnemyIds.Add(Entry.Definition->EnemyId);
	}

	OutDiagnostic.Reset();
	return true;
}
