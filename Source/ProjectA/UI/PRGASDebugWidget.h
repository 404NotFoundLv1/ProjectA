#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRGASDebugWidget.generated.h"

class STextBlock;
class UAbilitySystemComponent;

/**
 * Lightweight runtime GAS debug HUD for ProjectRift.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPRGASDebugWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ProjectRift|GAS|Debug")
	FString GetDebugText() const;

	/** Formats the live cooldown effects currently granted by the ASC. */
	static FString GetCooldownDebugText(const UAbilitySystemComponent* AbilitySystemComponent);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TSharedPtr<STextBlock> DebugTextBlock;
};
