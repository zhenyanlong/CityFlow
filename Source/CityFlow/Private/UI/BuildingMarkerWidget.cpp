#include "UI/BuildingMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Grid/Building.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

void UBuildingMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Txt_Marker && WidgetTree && !WidgetTree->RootWidget)
	{
		Txt_Marker = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Marker"));
		WidgetTree->RootWidget = Txt_Marker;
	}

	if (Txt_Marker)
	{
		Txt_Marker->SetJustification(ETextJustify::Center);
	}

	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ApplyMarkerAppearance();
}

TSharedRef<SWidget> UBuildingMarkerWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return Super::RebuildWidget();
	}

	return SAssignNew(FallbackTextBlock, STextBlock)
		.Text(bOnScreen ? OnScreenText : OffScreenText)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(FSlateColor(bOnScreen ? OnScreenColor : OffScreenColor));
}

void UBuildingMarkerWidget::InitializeMarker(ABuilding* InBuilding)
{
	RepresentedBuilding = InBuilding;
	ApplyMarkerAppearance();
}

void UBuildingMarkerWidget::SetMarkerScreenState(bool bInOnScreen, float DirectionAngleDegrees)
{
	bOnScreen = bInOnScreen;
	DirectionAngle = DirectionAngleDegrees;
	ApplyMarkerAppearance();
	OnMarkerScreenStateChanged_BP(bOnScreen, DirectionAngle);
}

void UBuildingMarkerWidget::ApplyMarkerAppearance()
{
	const FText MarkerText = bOnScreen ? OnScreenText : OffScreenText;
	const FLinearColor MarkerColor = bOnScreen ? OnScreenColor : OffScreenColor;

	if (Txt_Marker)
	{
		Txt_Marker->SetText(MarkerText);
		Txt_Marker->SetColorAndOpacity(FSlateColor(MarkerColor));
	}
	else if (FallbackTextBlock.IsValid())
	{
		FallbackTextBlock->SetText(MarkerText);
		FallbackTextBlock->SetColorAndOpacity(FSlateColor(MarkerColor));
	}

	SetRenderTransformAngle(bOnScreen ? 0.0f : DirectionAngle);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
