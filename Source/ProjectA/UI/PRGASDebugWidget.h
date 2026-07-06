#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRGASDebugWidget.generated.h"

class STextBlock;

/**
 * Lightweight runtime GAS debug HUD for ProjectRift.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPRGASDebugWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString BuildDebugText() const;

	TSharedPtr<STextBlock> DebugTextBlock;
};
