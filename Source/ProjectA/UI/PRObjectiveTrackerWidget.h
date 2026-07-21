#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRObjectiveTrackerWidget.generated.h"

class APlayerController;
class STextBlock;

/** Native text-first tracker backed exclusively by replicated APRRiftGameState summaries. */
UCLASS()
class PROJECTA_API UPRObjectiveTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForController(APlayerController* InController);
	FText GetObjectiveTrackerText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshText();
	TWeakObjectPtr<APlayerController> LocalController;
	TSharedPtr<STextBlock> TrackerText;
};
