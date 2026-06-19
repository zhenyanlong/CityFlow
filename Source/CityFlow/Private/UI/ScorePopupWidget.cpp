#include "UI/ScorePopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CityFlowScorePopupWidget"

void UScorePopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Txt_Score && WidgetTree && !WidgetTree->RootWidget)
	{
		Txt_Score = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Score"));
		WidgetTree->RootWidget = Txt_Score;
	}

	if (Txt_Score)
	{
		Txt_Score->SetJustification(ETextJustify::Center);
		ApplyText();
		ApplyColor();
	}
}

void UScorePopupWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bScreenPopupInitialized)
	{
		return;
	}

	Age += InDeltaTime;
	const float Duration = FMath::Max(Lifetime, KINDA_SMALL_NUMBER);
	const float NormalizedAge = FMath::Clamp(Age / Duration, 0.0f, 1.0f);

	UpdateProjectedPosition();
	SetScorePopupOpacity(1.0f - NormalizedAge);

	const float Scale = FMath::Lerp(StartScale, EndScale, FMath::Clamp(NormalizedAge * 3.0f, 0.0f, 1.0f));
	SetRenderScale(FVector2D(Scale, Scale));

	if (NormalizedAge >= 1.0f)
	{
		RemoveFromParent();
	}
}

TSharedRef<SWidget> UScorePopupWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget)
	{
		return Super::RebuildWidget();
	}

	return SAssignNew(FallbackTextBlock, STextBlock)
		.Text(CurrentText)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
		.Justification(ETextJustify::Center)
		.ColorAndOpacity(FSlateColor(BaseColor));
}

void UScorePopupWidget::SetScorePopup(int32 DeltaScore, FLinearColor InColor)
{
	BaseColor = InColor;
	FFormatNamedArguments Args;
	Args.Add(TEXT("Score"), FText::AsNumber(DeltaScore));
	CurrentText = DeltaScore >= 0
		? FText::Format(LOCTEXT("PositiveScoreFormat", "+{Score}"), Args)
		: FText::Format(LOCTEXT("NegativeScoreFormat", "{Score}"), Args);

	ApplyText();
	ApplyColor();
}

void UScorePopupWidget::SetScorePopupOpacity(float InOpacity)
{
	BaseColor.A = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	ApplyColor();
	SetRenderOpacity(BaseColor.A);
}

void UScorePopupWidget::InitializeScreenPopup(APlayerController* InPlayerController, FVector InWorldLocation, int32 DeltaScore, FLinearColor InColor)
{
	PlayerController = InPlayerController;
	WorldLocation = InWorldLocation + WorldOffset;
	Age = 0.0f;
	bScreenPopupInitialized = true;

	SetScorePopup(DeltaScore, InColor);
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderScale(FVector2D(StartScale, StartScale));
	SetScorePopupOpacity(1.0f);
	UpdateProjectedPosition();
}

void UScorePopupWidget::ApplyText()
{
	if (!Txt_Score)
	{
		if (FallbackTextBlock.IsValid())
		{
			FallbackTextBlock->SetText(CurrentText);
		}
		return;
	}

	Txt_Score->SetText(CurrentText);
}

void UScorePopupWidget::ApplyColor()
{
	if (!Txt_Score)
	{
		if (FallbackTextBlock.IsValid())
		{
			FallbackTextBlock->SetColorAndOpacity(FSlateColor(BaseColor));
		}
		return;
	}

	Txt_Score->SetColorAndOpacity(FSlateColor(BaseColor));
}

void UScorePopupWidget::UpdateProjectedPosition()
{
	APlayerController* PC = PlayerController.Get();
	if (!PC)
	{
		return;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PC,
		WorldLocation,
		ScreenPosition,
		true);

	if (!bProjected)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	const float Duration = FMath::Max(Lifetime, KINDA_SMALL_NUMBER);
	const float NormalizedAge = FMath::Clamp(Age / Duration, 0.0f, 1.0f);
	const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, NormalizedAge, 2.0f);
	const FVector2D PopupPosition = ScreenPosition + FVector2D(0.0f, -RiseDistance * EaseAlpha);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetPosition(PopupPosition);
	}
	else
	{
		SetPositionInViewport(PopupPosition, false);
	}
}

#undef LOCTEXT_NAMESPACE
