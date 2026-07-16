#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Roles/PRRoleTypes.h"
#include "PRRoleLoadoutWidget.generated.h"

class STextBlock;

/** Native, shipping-safe lobby panel for inspecting and applying a role module loadout. */
UCLASS()
class PROJECTA_API UPRRoleLoadoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPRRoleLoadoutWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void RefreshLoadoutDisplay();

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void RequestApplyLoadout();

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void RequestRestoreDefaults();

	UFUNCTION(BlueprintCallable, Category = "Roles|UI")
	void RequestClose();

	void HandleApplyResult(EPRRoleLoadoutApplyResult Result, const FString& Diagnostic);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply HandleApplyClicked();
	FReply HandleDefaultsClicked();
	FReply HandleCloseClicked();
	FText BuildLoadoutSummary() const;
	bool ResolveCanonicalDefaults(FName& OutRoleId, FPRRoleLoadout& OutLoadout) const;

	TSharedPtr<STextBlock> SummaryTextBlock;
	TSharedPtr<STextBlock> StatusTextBlock;
	FString LocalStatus;
};
