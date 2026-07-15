#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRWeaponHUDWidget.generated.h"

class APlayerController;
class STextBlock;

/** Native crosshair and ammo readout for the local weapon component. */
UCLASS()
class PROJECTA_API UPRWeaponHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void InitializeForController(APlayerController* InController);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshText();

	TWeakObjectPtr<APlayerController> LocalController;
	TSharedPtr<STextBlock> CrosshairText;
	TSharedPtr<STextBlock> AmmoText;
};
