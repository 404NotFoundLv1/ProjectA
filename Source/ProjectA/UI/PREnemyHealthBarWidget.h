#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PREnemyHealthBarWidget.generated.h"

class APREnemyCharacter;
class SProgressBar;
class STextBlock;

/**
 * Small world-space health label for basic rift enemies.
 */
UCLASS(Blueprintable)
class PROJECTA_API UPREnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy|Health Bar")
	void SetObservedEnemy(APREnemyCharacter* InObservedEnemy);

	UFUNCTION(BlueprintPure, Category = "Enemy|Health Bar")
	APREnemyCharacter* GetObservedEnemy() const { return ObservedEnemy.Get(); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	float GetObservedHealthPercent() const;
	FText BuildHealthText() const;
	void RefreshSlate();

	UPROPERTY(Transient)
	TWeakObjectPtr<APREnemyCharacter> ObservedEnemy;

	TSharedPtr<SProgressBar> HealthProgressBar;
	TSharedPtr<STextBlock> HealthTextBlock;
};
