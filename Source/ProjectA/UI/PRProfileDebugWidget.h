#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Persistence/PRProfileTypes.h"
#include "PRProfileDebugWidget.generated.h"

class STextBlock;
class SVerticalBox;

UCLASS()
class PROJECTA_API UPRProfileDebugWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void RefreshProfiles();
	void SetStatus(const FPRProfileOperationResult& Result);
	FReply HandleCreateProfile();
	FReply HandleContinueProfile(FGuid ProfileId);
	FReply HandleDeleteProfile(FGuid ProfileId);
	FReply HandleSaveCheckpoint();
	FReply HandleCreateLegacyProfile();
	FReply HandleCorruptPrimary();

	TSharedPtr<SVerticalBox> RootBox;
	TSharedPtr<STextBlock> StatusText;
};
