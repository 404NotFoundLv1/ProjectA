#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PRRiftSettlementTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "PRRiftSettlementWidget.generated.h"

class STextBlock;
class SWidget;

/**
 * Native settlement panel that reads the replicated rift settlement state.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPRRiftSettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rift|Settlement")
	void SetPreviewSettlementData(const FPRRiftSettlementData& InSettlementData);

	UFUNCTION(BlueprintCallable, Category = "Rift|Settlement")
	void ClearPreviewSettlementData();

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	FPRRiftSettlementData GetDisplayedSettlementData() const { return DisplayedSettlementData; }

	UFUNCTION(BlueprintPure, Category = "Rift|Settlement")
	FText GetDisplayedSettlementText() const;

	UFUNCTION(BlueprintCallable, Category = "Rift|Settlement")
	void SetPersonalSaveStatus(const FString& InStatus);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshSettlementFromGameState();
	void RefreshSettlementSlate();
	FText BuildSettlementText(const FPRRiftSettlementData& InSettlementData) const;
	static FSlateFontInfo GetSettlementFont(float Size);

	FPRRiftSettlementData DisplayedSettlementData;
	FString PersonalSaveStatus = TEXT("Waiting");
	bool bUsePreviewData = false;

	TSharedPtr<SWidget> SettlementRootWidget;
	TSharedPtr<STextBlock> SettlementTextBlock;
};
