#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "PRInteractionPromptWidget.generated.h"

class STextBlock;

/**
 * Screen-space prompt used by world interactables. Slate text rendering keeps CJK fallback fonts available.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPRInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction|Prompt")
	void SetPromptText(const FText& InPromptText);

	UFUNCTION(BlueprintPure, Category = "Interaction|Prompt")
	FText GetPromptText() const { return PromptText; }

	UFUNCTION(BlueprintCallable, Category = "Interaction|Prompt")
	void SetPromptColor(const FLinearColor& InPromptColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	FSlateFontInfo GetPromptFont() const;
	void RefreshPromptSlate();

	UPROPERTY(Transient)
	FText PromptText = FText::GetEmpty();

	UPROPERTY(Transient)
	FLinearColor PromptColor = FLinearColor(0.2f, 1.0f, 1.0f, 1.0f);

	TSharedPtr<STextBlock> PromptTextBlock;
};
