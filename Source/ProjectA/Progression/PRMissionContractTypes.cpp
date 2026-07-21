#include "Progression/PRMissionContractTypes.h"

#include "Misc/Crc.h"

namespace PRMissionContract
{
	static bool ValidateUniqueNames(const TArray<FName>& Names, const TCHAR* Label, FString* OutDiagnostic)
	{
		TSet<FName> Seen;
		for (const FName Name : Names)
		{
			if (Name.IsNone() || Seen.Contains(Name))
			{
				if (OutDiagnostic) { *OutDiagnostic = FString::Printf(TEXT("%s contain an invalid or duplicate id."), Label); }
				return false;
			}
			Seen.Add(Name);
		}
		return true;
	}

	static FString FindOption(const FString& Options, const TCHAR* Key)
	{
		TArray<FString> QuestionSegments;
		Options.ParseIntoArray(QuestionSegments, TEXT("?"), true);
		for (const FString& Segment : QuestionSegments)
		{
			TArray<FString> AmpersandSegments;
			Segment.ParseIntoArray(AmpersandSegments, TEXT("&"), true);
			for (const FString& Candidate : AmpersandSegments)
			{
				FString CandidateKey;
				FString CandidateValue;
				if (Candidate.Split(TEXT("="), &CandidateKey, &CandidateValue) && CandidateKey.Equals(Key, ESearchCase::CaseSensitive))
				{
					return CandidateValue;
				}
			}
		}
		return FString();
	}
}

bool FPRMissionRewardPreview::IsValid(FString* OutDiagnostic) const
{
	if (RewardTypeIds.IsEmpty() || !PRMissionContract::ValidateUniqueNames(RewardTypeIds, TEXT("Reward preview types"), OutDiagnostic))
	{
		if (OutDiagnostic && OutDiagnostic->IsEmpty()) { *OutDiagnostic = TEXT("Reward preview requires at least one type."); }
		return false;
	}
	if (MinimumBudget < 0 || MaximumBudget < MinimumBudget || MinimumRarity < 0 || MaximumRarity < MinimumRarity)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Reward preview ranges are invalid."); }
		return false;
	}
	return true;
}

bool FPRMissionContract::IsValid(FString* OutDiagnostic) const
{
	if (ContractVersion <= 0 || BiomeId.IsNone() || DifficultyId.IsNone())
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Contract version, biome id, and difficulty id are required."); }
		return false;
	}
	return PRMissionContract::ValidateUniqueNames(ModifierIds, TEXT("Contract modifiers"), OutDiagnostic) && RewardPreview.IsValid(OutDiagnostic);
}

bool FPRMissionDefinition::IsValid(FString* OutDiagnostic) const
{
	if (ContractId.IsNone() || ContractVersion <= 0 || BiomeId.IsNone() || DifficultyId.IsNone() || Seed == 0 || DeterministicSignature == 0)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission definition is incomplete."); }
		return false;
	}
	return PRMissionContract::ValidateUniqueNames(ModifierIds, TEXT("Mission definition modifiers"), OutDiagnostic) && RewardPreview.IsValid(OutDiagnostic);
}

bool FPRMissionTravelContext::IsValid(FString* OutDiagnostic) const
{
	if (ContractId.IsNone() || ContractVersion <= 0 || Seed == 0)
	{
		if (OutDiagnostic) { *OutDiagnostic = TEXT("Mission travel context is incomplete."); }
		return false;
	}
	return true;
}

FString FPRMissionTravelContext::ToTravelOptions() const
{
	return FString::Printf(TEXT("ContractId=%s?ContractVersion=%d?Seed=%d"), *ContractId.ToString(), ContractVersion, Seed);
}

bool FPRMissionTravelContext::ParseOptions(const FString& Options, FPRMissionTravelContext& OutContext, FString* OutDiagnostic)
{
	OutContext = FPRMissionTravelContext();
	const FString ContractText = PRMissionContract::FindOption(Options, TEXT("ContractId"));
	const FString LegacyMissionText = PRMissionContract::FindOption(Options, TEXT("MissionId"));
	OutContext.ContractId = FName(ContractText.IsEmpty() ? *LegacyMissionText : *ContractText);
	const FString VersionText = PRMissionContract::FindOption(Options, TEXT("ContractVersion"));
	const FString SeedText = PRMissionContract::FindOption(Options, TEXT("Seed"));
	OutContext.ContractVersion = VersionText.IsEmpty() ? 0 : FCString::Atoi(*VersionText);
	OutContext.Seed = SeedText.IsEmpty() ? 0 : FCString::Atoi(*SeedText);

	if (ContractText.IsEmpty() && !LegacyMissionText.IsEmpty())
	{
		// Legacy MissionId travel is intentionally parsed but remains incomplete;
		// the rift server fills its current contract version and a server seed.
		return true;
	}
	return OutContext.IsValid(OutDiagnostic);
}
