#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingMarkerWidget.generated.h"

class ABuilding;
class STextBlock;
class UTextBlock;

/**
 * Projects a building into screen space and switches to a rotated edge marker when
 * the target is outside the viewport. A Slate fallback keeps the feature usable
 * even when a Blueprint does not provide the optional bound text block.
 */
UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API UBuildingMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Building Marker")
	void InitializeMarker(ABuilding* InBuilding);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Building Marker")
	void SetMarkerScreenState(bool bInOnScreen, float DirectionAngleDegrees);

	UFUNCTION(BlueprintPure, Category = "CityFlow|Building Marker")
	ABuilding* GetRepresentedBuilding() const { return RepresentedBuilding.Get(); }

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|Building Marker")
	void OnMarkerScreenStateChanged_BP(bool bInOnScreen, float DirectionAngleDegrees);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	FText OnScreenText = NSLOCTEXT("CityFlowBuildingMarker", "DefaultOnScreenMarker", "B");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	FText OffScreenText = NSLOCTEXT("CityFlowBuildingMarker", "DefaultOffScreenMarker", ">");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	FLinearColor OnScreenColor = FLinearColor(0.1f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	FLinearColor OffScreenColor = FLinearColor(1.0f, 0.82f, 0.1f, 1.0f);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Marker;

private:
	void ApplyMarkerAppearance();

	TWeakObjectPtr<ABuilding> RepresentedBuilding;
	TSharedPtr<STextBlock> FallbackTextBlock;
	bool bOnScreen = true;
	float DirectionAngle = 0.0f;
};
