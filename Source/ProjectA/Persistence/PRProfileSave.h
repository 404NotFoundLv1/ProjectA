#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Persistence/PRProfileTypes.h"
#include "PRProfileSave.generated.h"

UCLASS(BlueprintType)
class PROJECTA_API UPRProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 InitialSaveVersion = 1;
	static constexpr int32 LatestSaveVersion = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	int32 SaveVersion = LatestSaveVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FGuid ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime CreatedUtc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	FDateTime LastSavedUtc;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	FPRProfileSnapshot Snapshot;

	FPRProfileOperationResult MigrateToLatest();
	bool IsValid(FString* OutDiagnostic = nullptr) const;
	FPRProfileSummary MakeSummary(bool bIsActive) const;
};

UCLASS()
class PROJECTA_API UPRProfileCatalogSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 LatestCatalogVersion = 1;

	UPROPERTY()
	int32 CatalogVersion = LatestCatalogVersion;

	UPROPERTY()
	FGuid ActiveProfileId;

	UPROPERTY()
	TArray<FGuid> ProfileIds;
};
