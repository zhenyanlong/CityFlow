#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingMarkerWidget.generated.h"

class ABuilding;
class STextBlock;
class UTextBlock;

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
