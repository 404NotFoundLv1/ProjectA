#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRQuickbarHUDWidget.generated.h"

class APlayerController;
class STextBlock;

/** Owner-local four-slot quickbar readout backed by APRPlayerState's replicated component. */
UCLASS()
class PROJECTA_API UPRQuickbarHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Quickbar|UI")
	void InitializeForController(APlayerController* InController);

	UFUNCTION(BlueprintPure, Category = "Quickbar|UI")
	FText GetQuickbarText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshText();
	TWeakObjectPtr<APlayerController> LocalController;
	TSharedPtr<STextBlock> QuickbarText;
};
