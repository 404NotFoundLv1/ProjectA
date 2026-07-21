#pragma once

#include "CoreMinimal.h"
#include "PRObjectiveGraphTypes.generated.h"

UENUM(BlueprintType)
enum class EPRObjectiveType : uint8
{
	Hold,
	Collect,
	Carry,
	Destroy,
	Hunt
};

UENUM(BlueprintType)
enum class EPRObjectiveNodeState : uint8
{
	Locked,
	Available,
	Active,
	Completed,
	Failed
};

UENUM(BlueprintType)
enum class EPRObjectivePrerequisitePolicy : uint8
{
	All,
	Any
};

UENUM(BlueprintType)
enum class EPRObjectiveActivationMode : uint8
{
	Manual,
	Automatic
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRObjectiveNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName NodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EPRObjectiveType ObjectiveType = EPRObjectiveType::Hold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TArray<FName> PrerequisiteNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EPRObjectivePrerequisitePolicy PrerequisitePolicy = EPRObjectivePrerequisitePolicy::All;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EPRObjectiveActivationMode ActivationMode = EPRObjectiveActivationMode::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bDrivesEnemySpawning = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName TargetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "1"))
	int32 TargetCount = 1;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRObjectiveGraphDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 GraphVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	TArray<FPRObjectiveNodeDefinition> Nodes;

	bool IsValid(FString* OutDiagnostic = nullptr) const;
	int32 ComputeSignature() const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRObjectiveSummary
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	FName NodeId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	EPRObjectiveType ObjectiveType = EPRObjectiveType::Hold;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	EPRObjectiveNodeState State = EPRObjectiveNodeState::Locked;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 CurrentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	int32 TargetCount = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	float Progress = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRObjectiveNodeSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FName NodeId = NAME_None;

	UPROPERTY()
	EPRObjectiveNodeState State = EPRObjectiveNodeState::Locked;

	UPROPERTY()
	int32 CurrentCount = 0;

	UPROPERTY()
	int32 RecoveryAttempts = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FPRObjectiveGraphSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	int32 GraphVersion = 0;

	UPROPERTY()
	int32 GraphSignature = 0;

	UPROPERTY()
	TArray<FPRObjectiveNodeSnapshot> Nodes;

	bool IsValid() const;
};
