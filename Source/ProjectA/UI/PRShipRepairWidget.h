#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRShipRepairWidget.generated.h"

class STextBlock;

/** Formal native ship repair panel used in both Development and Shipping builds. */
UCLASS()
class PROJECTA_API UPRShipRepairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPRShipRepairWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void RefreshRepairDisplay();

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void RequestDisplayedRepair();

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|UI")
	void RequestClose();

	UFUNCTION(BlueprintPure, Category = "Ship Repair|UI")
	bool IsAcceptancePreparationAvailable() const;

	UFUNCTION(BlueprintCallable, Category = "Ship Repair|Development")
	void PrepareAcceptanceProfile();

	UFUNCTION(BlueprintPure, Category = "Ship Repair|UI")
	FName GetDisplayedRepairProjectId() const { return DisplayedRepairProjectId; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply HandleRepairClicked();
	FReply HandleCloseClicked();
	FReply HandlePrepareClicked();
	FText BuildRepairSummary() const;
	FText BuildRepairStatus() const;
	bool CanRequestRepair() const;

	UPROPERTY(EditDefaultsOnly, Category = "Ship Repair")
	FName DisplayedRepairProjectId = FName(TEXT("Repair.Ship.Engine.Stage1"));

	TSharedPtr<STextBlock> SummaryTextBlock;
	TSharedPtr<STextBlock> StatusTextBlock;
	FString LocalStatus;
};
