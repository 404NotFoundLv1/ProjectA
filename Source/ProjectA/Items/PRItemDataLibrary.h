#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/PRItemTypes.h"
#include "PRItemDataLibrary.generated.h"

class UPRItemDataAsset;

UCLASS()
class PROJECTA_API UPRItemDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Items")
	static UPRItemDataAsset* FindItemDataById(const TArray<UPRItemDataAsset*>& ItemDataAssets, FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Items")
	static bool IsValidItemInstance(const FPRItemInstance& ItemInstance);
};
