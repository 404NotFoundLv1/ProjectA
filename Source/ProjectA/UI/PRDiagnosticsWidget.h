#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Diagnostics/PRDeveloperToolsTypes.h"
#include "PRDiagnosticsWidget.generated.h"

class APlayerController;
class STextBlock;
class SVerticalBox;

UENUM(BlueprintType)
enum class EPRDiagnosticsTab : uint8
{
	Overview,
	Player,
	TeamAndRift,
	Events,
	Tools
};

UCLASS()
class PROJECTA_API UPRDiagnosticsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void InitializeForController(APlayerController* InLocalController, EPRDiagnosticsTab InitialTab = EPRDiagnosticsTab::Overview);

	UFUNCTION(BlueprintCallable, Category="Diagnostics")
	void SetActiveTab(EPRDiagnosticsTab NewTab);

	UFUNCTION(BlueprintPure, Category="Diagnostics")
	EPRDiagnosticsTab GetActiveTab() const { return ActiveTab; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FReply HandleClose();
	FReply HandleTabSelected(EPRDiagnosticsTab NewTab);
	FReply HandleDeveloperAction(EPRDeveloperAction Action, FGuid ProfileId = FGuid());
	FReply HandleExport();
	FReply HandleClearEvents();
	void RefreshBody();
	void RefreshTools();
	FString BuildBodyText() const;
	bool IsMatchingConfirmation(EPRDeveloperAction Action, const FGuid& ProfileId) const;
	void ClearConfirmation();

	TWeakObjectPtr<APlayerController> LocalController;
	EPRDiagnosticsTab ActiveTab = EPRDiagnosticsTab::Overview;
	EPRDeveloperAction PendingConfirmationAction = EPRDeveloperAction::CreateProfile;
	FGuid PendingConfirmationProfileId;
	bool bConfirmationPending = false;
	double RefreshAccumulator = 0.0;
	double ToolsRefreshAccumulator = 0.0;
	FString StatusMessage;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> BodyText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SVerticalBox> ToolsBox;
};
