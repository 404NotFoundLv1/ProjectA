#pragma once

#include "CoreMinimal.h"
#include "Enemies/PREnemyDefinitionDataAsset.h"
#include "Engine/DataAsset.h"
#include "Progression/PRRiftRuleTypes.h"
#include "PREnemyRosterDataAsset.generated.h"

class APREnemyCharacter;

USTRUCT(BlueprintType)
struct PROJECTA_API FPREnemyRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") TObjectPtr<UPREnemyDefinitionDataAsset> Definition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") TSubclassOf<APREnemyCharacter> EnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") float SelectionWeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") int32 PackSize = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") int32 MaxAlive = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") EPRRiftRiskTier MinimumRiskTier = EPRRiftRiskTier::Stable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") int32 MinimumFrozenPlayerCount = 1;
};

UCLASS(BlueprintType)
class PROJECTA_API UPREnemyRosterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType EnemyRosterPrimaryAssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool ValidateRoster(FString& OutDiagnostic) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") FName RosterId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roster") TArray<FPREnemyRosterEntry> Entries;
};
