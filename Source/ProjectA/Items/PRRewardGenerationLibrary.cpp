#include "Items/PRRewardGenerationLibrary.h"

#include "Items/PRLootTableDataAsset.h"
#include "Items/PRRewardBudgetDataAsset.h"
#include "Misc/Crc.h"

int32 UPRRewardGenerationLibrary::DeriveSeed(const int32 RunSeed, const FPRRewardSourceContext& Source)
{
	const FString Input = FString::Printf(TEXT("ProjectRift.Rewards.v7|%d|%d|%s|%s|%d"), RunSeed, static_cast<int32>(Source.SourceType), *Source.SourceId.ToString(), *Source.RecipientProfileId.ToString(EGuidFormats::Digits), Source.Ordinal);
	return static_cast<int32>(FCrc::StrCrc32(*Input));
}

FPRPersonalRewardGenerationResult UPRRewardGenerationLibrary::GeneratePersonalSettlementReward(
	const UPRRewardBudgetDataAsset* RewardBudget,
	const FPRRewardSourceContext& Source,
	const FPRLootProtectionState& PreviousProtection,
	const int32 FrozenParticipantCount)
{
	return GeneratePersonalSettlementRewardWithModifiers(RewardBudget, Source, PreviousProtection, FrozenParticipantCount, FPRRewardRuntimeModifiers());
}

FPRPersonalRewardGenerationResult UPRRewardGenerationLibrary::GeneratePersonalSettlementRewardWithModifiers(
	const UPRRewardBudgetDataAsset* RewardBudget,
	const FPRRewardSourceContext& Source,
	const FPRLootProtectionState& PreviousProtection,
	const int32 FrozenParticipantCount,
	const FPRRewardRuntimeModifiers& RuntimeModifiers)
{
	FPRPersonalRewardGenerationResult Result;
	Result.UpdatedProtectionState = PreviousProtection;
	if (!RewardBudget || !Source.RecipientProfileId.IsValid())
	{
		Result.Diagnostic = TEXT("Reward generation requires a valid budget and recipient.");
		return Result;
	}
	FString Validation;
	if (!RewardBudget->IsValidRewardBudget(&Validation))
	{
		Result.Diagnostic = Validation;
		return Result;
	}
	const int32 BaseBudget = RewardBudget->BaseSuccessBudget + RewardBudget->ObjectiveBudget
		+ RewardBudget->AdditionalPlayerBudget * FMath::Clamp(FrozenParticipantCount - 1, 0, 3);
	const float RewardMultiplier = FMath::Clamp(FMath::IsFinite(RuntimeModifiers.Multiplier) ? RuntimeModifiers.Multiplier : 1.0f, 0.25f, 3.0f);
	Result.TotalBudget = FMath::Max(0, FMath::RoundToInt(BaseBudget * RewardMultiplier) + FMath::Max(0, RuntimeModifiers.FlatBonusBudget));
	Result.RemainingBudget = Result.TotalBudget;
	Result.UpdatedProtectionState.RewardBudgetId = RewardBudget->GetFName();
	UPRLootTableDataAsset* Table = RewardBudget->PersonalEquipmentLootTable.LoadSynchronous();
	FPRItemInstance Item;
	FString Diagnostic;
	const int32 Seed = Source.Seed != 0 ? Source.Seed : DeriveSeed(0, Source);
	const EPRItemRarity MinimumRarity = PreviousProtection.ConsecutiveBelowRare >= RewardBudget->RarePityThreshold
		? EPRItemRarity::Rare
		: EPRItemRarity::Common;
	const FName ExcludedItemId = PreviousProtection.ConsecutiveSameItem >= 2 ? PreviousProtection.LastItemId : NAME_None;
	if (Result.RemainingBudget >= RewardBudget->EquipmentCost && Table
		&& Table->RollSeededLootFiltered(Seed, ExcludedItemId, MinimumRarity, Item, Diagnostic) && Item.IsValid())
	{
		Item.InstanceGuid = FGuid::NewGuid();
		Result.GrantedWarehouseItems.Add(Item);
		Result.RemainingBudget -= RewardBudget->EquipmentCost;
		Result.UpdatedProtectionState.LastItemId = Item.ItemId;
		Result.UpdatedProtectionState.ConsecutiveSameItem = PreviousProtection.LastItemId == Item.ItemId ? PreviousProtection.ConsecutiveSameItem + 1 : 1;
		Result.UpdatedProtectionState.ConsecutiveBelowRare = Item.Rarity < EPRItemRarity::Rare ? PreviousProtection.ConsecutiveBelowRare + 1 : 0;
		FPRRewardAuditEntry& Audit = Result.AuditEntries.AddDefaulted_GetRef();
		Audit.Source = Source;
		Audit.Source.Seed = Seed;
		Audit.RewardBudgetId = RewardBudget->GetFName();
		Audit.BudgetSpent = RewardBudget->EquipmentCost;
		Audit.GrantedItem = Item;
	}
	Result.CommonChipCount = Result.RemainingBudget / RewardBudget->CommonChipCost;
	Result.bSuccess = true;
	return Result;
}
