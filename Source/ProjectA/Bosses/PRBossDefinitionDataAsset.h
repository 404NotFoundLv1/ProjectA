#pragma once

#include "Bosses/PRBossTypes.h"
#include "Engine/DataAsset.h"
#include "PRBossDefinitionDataAsset.generated.h"

class APRBossCharacter;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRBossAttributeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attributes", meta = (ClampMin = "1.0")) float MaxHealth = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attributes", meta = (ClampMin = "0.0")) float InitialShield = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attributes", meta = (ClampMin = "0.0")) float MaxShield = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attributes", meta = (ClampMin = "0.0")) float AttackPower = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Attributes", meta = (ClampMin = "0.0")) float MoveSpeed = 300.0f;
};

UCLASS(BlueprintType)
class PROJECTA_API UPRBossDefinitionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType BossPrimaryAssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool ValidateDefinition(FString& OutDiagnostic) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") FName BossId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TSubclassOf<APRBossCharacter> BossClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") FPRBossAttributeDefinition Attributes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling") FPRBossPlayerCountScalar HealthScaling;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling") FPRBossPlayerCountScalar DamageScaling;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Scaling") FPRBossPlayerCountScalar StaggerScaling;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TArray<FPRBossPhaseDefinition> Phases;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TArray<FPRBossAbilityPatternDefinition> AbilityPatterns;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TArray<FPRBossWeakPointDefinition> WeakPoints;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") TArray<FPRBossArenaEventDefinition> ArenaEvents;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss") FName RewardId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0")) int32 RecentPatternRepeatWindow = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0.0")) float StaggerDurationSeconds = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (ClampMin = "0.0")) float StaggerGraceSeconds = 3.0f;
};
