#include "Production/PRProductionValidationTypes.h"

const FPRProductionAssetRule* FPRProductionPolicy::FindAssetRule(const FString& ClassName) const
{
	return AssetRules.FindByPredicate([&ClassName](const FPRProductionAssetRule& Rule)
	{
		return Rule.ClassName.Equals(ClassName, ESearchCase::CaseSensitive);
	});
}

const FPRLicenseRecord* FPRLicenseRegistry::FindCoverage(const FString& AssetPath) const
{
	const FPRLicenseRecord* BestFolderRecord = nullptr;
	for (const FPRLicenseRecord& Record : Records)
	{
		if (!Record.bFolderCoverage && Record.AssetPath.Equals(AssetPath, ESearchCase::CaseSensitive))
		{
			return &Record;
		}

		const FString FolderPrefix = Record.AssetPath.EndsWith(TEXT("/")) ? Record.AssetPath : Record.AssetPath + TEXT("/");
		if (Record.bFolderCoverage && AssetPath.StartsWith(FolderPrefix, ESearchCase::CaseSensitive)
			&& (!BestFolderRecord || Record.AssetPath.Len() > BestFolderRecord->AssetPath.Len()))
		{
			BestFolderRecord = &Record;
		}
	}
	return BestFolderRecord;
}
