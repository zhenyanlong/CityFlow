#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScorePopupWidget.generated.h"

class UTextBlock;
class STextBlock;
class APlayerController;

UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API UScorePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Score Popup")
	void SetScorePopup(int32 DeltaScore, FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Score Popup")
	void SetScorePopupOpacity(float InOpacity);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Score Popup")
	void InitializeScreenPopup(APlayerController* InPlayerController, FVector InWorldLocation, int32 DeltaScore, FLinearColor InColor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	float Lifetime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	float RiseDistance = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	FVector WorldOffset = FVector(0.0f, 0.0f, 140.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	float StartScale = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	float EndScale = 1.0f;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Score;

private:
	void ApplyText();
	void ApplyColor();
	void UpdateProjectedPosition();

	FText CurrentText = NSLOCTEXT("CityFlowScorePopup", "DefaultScorePopup", "+0");
	FLinearColor BaseColor = FLinearColor::White;
	TSharedPtr<STextBlock> FallbackTextBlock;
	TWeakObjectPtr<APlayerController> PlayerController;
	FVector WorldLocation = FVector::ZeroVector;
	float Age = 0.0f;
	bool bScreenPopupInitialized = false;
};
