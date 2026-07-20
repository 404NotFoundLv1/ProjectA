#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRCraftingWidget.generated.h"

/** Temporary native lobby crafting panel. Blueprint may supply the visual layout without owning state. */
UCLASS(BlueprintType)
class PROJECTA_API UPRCraftingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPRCraftingWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CraftRecipe(FName RecipeId);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void DismantleItem(FGuid InstanceGuid);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void UpgradeItem(FGuid InstanceGuid);

	UFUNCTION(BlueprintPure, Category = "Crafting")
	FString GetStatusText() const;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void RefreshCraftingDisplay();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply HandleCraftClicked(FName RecipeId);
	FReply HandleNextItemClicked();
	FReply HandleDismantleClicked();
	FReply HandleUpgradeClicked();
	FReply HandleCloseClicked();
	FText BuildSummary() const;
	FText BuildStatus() const;
	void SelectNextUpgradeableItem();

	TSharedPtr<class STextBlock> SummaryTextBlock;
	TSharedPtr<class STextBlock> StatusTextBlock;
	FGuid SelectedItemGuid;
	FString LocalStatus;
};
