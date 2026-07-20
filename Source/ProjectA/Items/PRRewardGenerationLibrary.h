#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/PRRewardTypes.h"
#include "PRRewardGenerationLibrary.generated.h"

class UPRRewardBudgetDataAsset;

/** Deterministic, server-only reward generation. No client-provided entropy is accepted. */
UCLASS()
class PROJECTA_API UPRRewardGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Rewards")
	static int32 DeriveSeed(int32 RunSeed, const FPRRewardSourceContext& Source);

	UFUNCTION(BlueprintCallable, Category = "Rewards")
	static FPRPersonalRewardGenerationResult GeneratePersonalSettlementReward(
		const UPRRewardBudgetDataAsset* RewardBudget,
		const FPRRewardSourceContext& Source,
		const FPRLootProtectionState& PreviousProtection,
		int32 FrozenParticipantCount);
};
