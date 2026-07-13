#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRLobbyReadyDebugWidget.generated.h"

class STextBlock;

UCLASS()
class PROJECTA_API UPRLobbyReadyDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetReadyText(const FText& InText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<STextBlock> ReadyTextBlock;
	FText PendingText;
};
