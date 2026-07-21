#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRBossHUDWidget.generated.h"

class APlayerController;
class STextBlock;
class SProgressBar;

/** Minimal replicated Boss encounter readout used by the framework test arena. */
UCLASS()
class PROJECTA_API UPRBossHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Boss|UI") void InitializeForController(APlayerController* InController);
	UFUNCTION(BlueprintPure, Category = "Boss|UI") FText GetBossText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshText();
	TWeakObjectPtr<APlayerController> LocalController;
	TSharedPtr<STextBlock> BossText;
	TSharedPtr<SProgressBar> HealthBar;
};
