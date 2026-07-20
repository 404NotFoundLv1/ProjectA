#include "Items/PRAffixGenerationLibrary.h"

#include "Abilities/PRAffixGameplayEffects.h"
#include "Items/PRAffixDefinitionDataAsset.h"
#include "Items/PREquipmentDataAsset.h"
#include "Misc/Crc.h"

namespace
{
int32 DeriveSeed(const int32 Seed, const FString& Salt)
{
	const uint32 Hash = FCrc::StrCrc32(*FString::Printf(TEXT("ProjectRift.Affix.v1|%d|%s"), Seed, *Salt));
	return static_cast<int32>(Hash);
}

bool IsGeneratedRarity(const EPRItemRarity Rarity)
{
	return Rarity == EPRItemRarity::Common
		|| Rarity == EPRItemRarity::Uncommon
		|| Rarity == EPRItemRarity::Rare
		|| Rarity == EPRItemRarity::Prototype;
}

const FPRRarityGenerationRule* SelectRarityRule(const TArray<FPRRarityGenerationRule>& Rules, FRandomStream& Stream)
{
	float TotalWeight = 0.0f;
	for (const FPRRarityGenerationRule& Rule : Rules)
	{
		TotalWeight += Rule.Weight;
	}
	const float Roll = Stream.FRandRange(0.0f, TotalWeight);
	float Accumulated = 0.0f;
	for (const FPRRarityGenerationRule& Rule : Rules)
	{
		Accumulated += Rule.Weight;
		if (Roll < Accumulated)
		{
			return &Rule;
		}
	}
	return Rules.IsEmpty() ? nullptr : &Rules.Last();
}

bool IsDefinitionCompatible(const UPRAffixDefinitionDataAsset& Definition, const EPREquipmentSlot Slot)
{
	FString Diagnostic;
	if (!Definition.ValidateDefinition(Diagnostic) || !Definition.AllowedSlots.Contains(Slot))
	{
		return false;
	}
	const UPRAffixModifierGameplayEffect* Effect = Definition.ModifierEffectClass
		? Cast<UPRAffixModifierGameplayEffect>(Definition.ModifierEffectClass->GetDefaultObject())
		: nullptr;
	return Effect && Effect->GetAffixAttribute() == Definition.Attribute && Effect->GetAffixModifierType() == Definition.ModifierType;
}

const UPRAffixDefinitionDataAsset* SelectDefinition(
	const TArray<const UPRAffixDefinitionDataAsset*>& Candidates,
	FRandomStream& Stream)
{
	float TotalWeight = 0.0f;
	for (const UPRAffixDefinitionDataAsset* Candidate : Candidates)
	{
		TotalWeight += Candidate->Weight;
	}
	const float Roll = Stream.FRandRange(0.0f, TotalWeight);
	float Accumulated = 0.0f;
	for (const UPRAffixDefinitionDataAsset* Candidate : Candidates)
	{
		Accumulated += Candidate->Weight;
		if (Roll < Accumulated)
		{
			return Candidate;
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates.Last();
}

float QuantizeAffixMagnitude(const float Value)
{
	return FMath::RoundToFloat(Value * 10000.0f) / 10000.0f;
}
}

TArray<FPRRarityGenerationRule> UPRAffixGenerationLibrary::MakeDefaultRarityRules()
{
	return {
		{ EPRItemRarity::Common, 60.0f, 0 },
		{ EPRItemRarity::Uncommon, 25.0f, 1 },
		{ EPRItemRarity::Rare, 12.0f, 2 },
		{ EPRItemRarity::Prototype, 3.0f, 3 }
	};
}

bool UPRAffixGenerationLibrary::ValidateRarityRules(const TArray<FPRRarityGenerationRule>& Rules, FString& OutDiagnostic)
{
	OutDiagnostic.Reset();
	if (Rules.Num() != 4)
	{
		OutDiagnostic = TEXT("Affix generation requires exactly four rarity rules.");
		return false;
	}
	TSet<EPRItemRarity> SeenRarities;
	for (const FPRRarityGenerationRule& Rule : Rules)
	{
		if (!Rule.IsValid() || !IsGeneratedRarity(Rule.Rarity) || SeenRarities.Contains(Rule.Rarity))
		{
			OutDiagnostic = TEXT("Rarity rules contain an invalid, legacy, or duplicate entry.");
			return false;
		}
		SeenRarities.Add(Rule.Rarity);
	}
	for (const EPRItemRarity Required : { EPRItemRarity::Common, EPRItemRarity::Uncommon, EPRItemRarity::Rare, EPRItemRarity::Prototype })
	{
		if (!SeenRarities.Contains(Required))
		{
			OutDiagnostic = TEXT("Rarity rules omit a required generated rarity.");
			return false;
		}
	}
	return true;
}

FPRAffixGenerationResult UPRAffixGenerationLibrary::GenerateEquipmentInstance(
	const UPREquipmentDataAsset* EquipmentDefinition,
	const int32 LootSeed,
	const TArray<UPRAffixDefinitionDataAsset*>& Definitions,
	const TArray<FPRRarityGenerationRule>& RarityRules)
{
	FPRAffixGenerationResult Result;
	if (!EquipmentDefinition || EquipmentDefinition->ItemId.IsNone() || EquipmentDefinition->EquipmentSlot == EPREquipmentSlot::None)
	{
		Result.Diagnostic = TEXT("Equipment definition is missing an item id or concrete equipment slot.");
		return Result;
	}
	if (!ValidateRarityRules(RarityRules, Result.Diagnostic))
	{
		return Result;
	}

	FRandomStream RarityStream(DeriveSeed(LootSeed, EquipmentDefinition->ItemId.ToString() + TEXT("|Rarity")));
	const FPRRarityGenerationRule* RarityRule = SelectRarityRule(RarityRules, RarityStream);
	if (!RarityRule)
	{
		Result.Diagnostic = TEXT("No rarity rule could be selected.");
		return Result;
	}

	TArray<const UPRAffixDefinitionDataAsset*> AllCandidates;
	TSet<FName> SeenIds;
	for (const UPRAffixDefinitionDataAsset* Definition : Definitions)
	{
		if (!Definition || SeenIds.Contains(Definition->AffixId) || !IsDefinitionCompatible(*Definition, EquipmentDefinition->EquipmentSlot))
		{
			continue;
		}
		SeenIds.Add(Definition->AffixId);
		AllCandidates.Add(Definition);
	}
	AllCandidates.Sort([](const UPRAffixDefinitionDataAsset& A, const UPRAffixDefinitionDataAsset& B)
	{
		return A.AffixId.LexicalLess(B.AffixId);
	});

	Result.Item.ItemId = EquipmentDefinition->ItemId;
	Result.Item.Count = 1;
	Result.Item.Level = 1;
	Result.Item.Durability = 1.0f;
	Result.Item.Rarity = RarityRule->Rarity;
	Result.Item.LootSeed = LootSeed;
	Result.Item.AffixGenerationVersion = CurrentGenerationVersion;
	if (RarityRule->AffixCount == 0)
	{
		Result.bSuccess = true;
		return Result;
	}

	FRandomStream SelectionStream(DeriveSeed(LootSeed, EquipmentDefinition->ItemId.ToString() + TEXT("|Selection")));
	FGameplayTagContainer UsedExclusionTags;
	for (int32 Index = 0; Index < RarityRule->AffixCount; ++Index)
	{
		TArray<const UPRAffixDefinitionDataAsset*> EligibleCandidates;
		for (const UPRAffixDefinitionDataAsset* Candidate : AllCandidates)
		{
			if (Candidate && !Result.Item.Affixes.Contains(Candidate->AffixId)
				&& !Candidate->MutuallyExclusiveTags.HasAny(UsedExclusionTags))
			{
				EligibleCandidates.Add(Candidate);
			}
		}
		const UPRAffixDefinitionDataAsset* Selected = SelectDefinition(EligibleCandidates, SelectionStream);
		if (!Selected)
		{
			Result.Diagnostic = TEXT("Not enough compatible affix definitions for the requested rarity.");
			Result.Item = FPRItemInstance();
			return Result;
		}
		FRandomStream MagnitudeStream(DeriveSeed(LootSeed, EquipmentDefinition->ItemId.ToString() + TEXT("|Magnitude|") + Selected->AffixId.ToString()));
		FPRAffixRoll& Roll = Result.Item.RolledAffixes.AddDefaulted_GetRef();
		Roll.AffixId = Selected->AffixId;
		Roll.Attribute = Selected->Attribute;
		Roll.ModifierType = Selected->ModifierType;
		Roll.Magnitude = QuantizeAffixMagnitude(MagnitudeStream.FRandRange(Selected->MinMagnitude, Selected->MaxMagnitude));
		Result.Item.Affixes.Add(Roll.AffixId);
		UsedExclusionTags.AppendTags(Selected->MutuallyExclusiveTags);
	}
	Result.bSuccess = true;
	return Result;
}

