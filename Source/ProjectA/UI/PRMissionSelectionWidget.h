#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Progression/PRMissionContractTypes.h"
#include "PRMissionSelectionWidget.generated.h"

class STextBlock;

USTRUCT(BlueprintType)
struct PROJECTA_API FPRMissionSelectionRow
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Mission") FName ContractId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Mission") FName BiomeId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Mission") FName DifficultyId = NAME_None;
	UPROPERTY(BlueprintReadOnly, Category="Mission") TArray<FName> ModifierIds;
	UPROPERTY(BlueprintReadOnly, Category="Mission") FPRMissionRewardPreview RewardPreview;
	UPROPERTY(BlueprintReadOnly, Category="Mission") bool bLocked = true;
};

/** Text-first fallback mission table; the server still owns selection and Seed generation. */
UCLASS()
class PROJECTA_API UPRMissionSelectionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPRMissionSelectionWidget(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintCallable, Category="Mission|UI") void RefreshMissionDisplay();
	UFUNCTION(BlueprintPure, Category="Mission|UI") const TArray<FPRMissionSelectionRow>& GetRows() const { return Rows; }
	UFUNCTION(BlueprintPure, Category="Mission|UI") int32 GetFocusedRowIndex() const { return FocusedRowIndex; }
	UFUNCTION(BlueprintCallable, Category="Mission|UI") void FocusNext();
	UFUNCTION(BlueprintCallable, Category="Mission|UI") void FocusPrevious();
	UFUNCTION(BlueprintCallable, Category="Mission|UI") void RequestFocusedSelection();
	UFUNCTION(BlueprintCallable, Category="Mission|UI") void RequestClose();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
private:
	FText BuildSummary() const;
	FReply HandlePrevious(); FReply HandleNext(); FReply HandleSelect(); FReply HandleClose();
	TArray<FPRMissionSelectionRow> Rows;
	int32 FocusedRowIndex = INDEX_NONE;
	TSharedPtr<STextBlock> SummaryText;
};
