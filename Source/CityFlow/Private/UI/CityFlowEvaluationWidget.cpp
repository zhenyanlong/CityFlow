#include "UI/CityFlowEvaluationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "CityFlowEvaluationWidget"

int32 UCityFlowEvaluationWidget::GlobalHighScore = 0;

void UCityFlowEvaluationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureScoreReportTextBlocks();

	if (Btn_BackToMain)
		Btn_BackToMain->OnClicked.AddDynamic(this, &UCityFlowEvaluationWidget::HandleBackToMainClicked);
	if (Btn_Restart)
		Btn_Restart->OnClicked.AddDynamic(this, &UCityFlowEvaluationWidget::HandleRestartClicked);
}

void UCityFlowEvaluationWidget::NativeDestruct()
{
	StopScoreAnimation(false);

	if (Btn_BackToMain)
		Btn_BackToMain->OnClicked.RemoveAll(this);
	if (Btn_Restart)
		Btn_Restart->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowEvaluationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bScoreAnimationActive)
	{
		AdvanceScoreAnimation(InDeltaTime);
	}
}

void UCityFlowEvaluationWidget::Populate(int32 TotalScore, int32 Arrivals, int32 Penalty, float ElapsedTime)
{
	bHasScoreBreakdown = false;
	CachedTotalScore = TotalScore;
	CachedArrivals = Arrivals;
	CachedPenalty = Penalty;
	CachedElapsedTime = ElapsedTime;

	if (TotalScore > GlobalHighScore)
		GlobalHighScore = TotalScore;

	BuildAnimatedScoreLines();
	StartScoreAnimation();
}

void UCityFlowEvaluationWidget::PopulateFromBreakdown(const FCityFlowScoreBreakdown& Breakdown)
{
	bHasScoreBreakdown = true;
	CachedBreakdown = Breakdown;
	CachedTotalScore = Breakdown.FinalScore;
	CachedArrivals = Breakdown.ArrivedVehicles;
	CachedPenalty = Breakdown.DeadVehicles;
	CachedElapsedTime = Breakdown.ElapsedSimulationTime;

	if (CachedTotalScore > GlobalHighScore)
		GlobalHighScore = CachedTotalScore;

	BuildAnimatedScoreLines();
	StartScoreAnimation();
}

void UCityFlowEvaluationWidget::RefreshUI()
{
	BuildAnimatedScoreLines();
	StopScoreAnimation(true);
}

void UCityFlowEvaluationWidget::BuildAnimatedScoreLines()
{
	EnsureScoreReportTextBlocks();
	AnimatedScoreLines.Empty();

	AddAnimatedLine(Txt_TotalScore, LOCTEXT("FinalScoreLabel", "Final Score: "), CachedTotalScore);

	if (bHasScoreBreakdown)
	{
		const auto MakeOutOfSuffix = [](int32 Total)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Total"), FText::AsNumber(Total));
			return FText::Format(LOCTEXT("OutOfSuffixFormat", " / {Total}"), Args);
		};

		AddAnimatedStarLine(LOCTEXT("RatingLabel", "Rating: "), CalculateStarRating());
		AddAnimatedLine(Txt_RawScore, LOCTEXT("RawScoreLabel", "Raw Score: "), CachedBreakdown.RawScore, 0);

		AddAnimatedLine(Txt_ConnectedBuildings, LOCTEXT("ConnectedBuildingsLabel", "Connected Buildings: "), CachedBreakdown.ConnectedBuildingCount, 0, FText::GetEmpty(), MakeOutOfSuffix(CachedBreakdown.TotalBuildingCount));
		AddAnimatedLine(Txt_LargestConnectedNetwork, LOCTEXT("LargestNetworkLabel", "Largest Connected Network: "), CachedBreakdown.LargestConnectedBuildingComponent, 0, FText::GetEmpty(), MakeOutOfSuffix(CachedBreakdown.TotalBuildingCount));
		AddAnimatedLine(Txt_BudgetUsed, LOCTEXT("BudgetUsedLabel", "Budget Used: "), CachedBreakdown.UsedBudget, 0, FText::GetEmpty(), MakeOutOfSuffix(CachedBreakdown.TotalRoadBudget));
		AddAnimatedLine(Txt_EstimatedMinimumRoadNeed, LOCTEXT("EstimatedRoadNeedLabel", "Estimated Minimum Road Need: "), CachedBreakdown.EstimatedMinRoadNeed);

		AddAnimatedLine(Txt_Arrivals, LOCTEXT("ArrivalsLabel", "Arrivals: "), CachedBreakdown.ArrivedVehicles, 0, FText::GetEmpty(), MakeOutOfSuffix(CachedBreakdown.SpawnedVehicles));
		AddAnimatedLine(Txt_Deaths ? Txt_Deaths.Get() : Txt_Penalty.Get(), LOCTEXT("DeathsLabel", "Deaths: "), CachedBreakdown.DeadVehicles);
		AddAnimatedLine(Txt_ArrivalRate, LOCTEXT("ArrivalRateLabel", "Arrival Rate: "), CachedBreakdown.ArrivalRate * 100.0, 0, FText::GetEmpty(), LOCTEXT("PercentSuffix", "%"));
		AddAnimatedLine(Txt_AverageCellTravelTime, LOCTEXT("AverageCellTimeLabel", "Average Cell Travel Time: "), CachedBreakdown.AverageCellTravelTime, 2, FText::GetEmpty(), LOCTEXT("SecondsSuffix", "s"));

		AddAnimatedLine(Txt_ConnectivityScore, LOCTEXT("ConnectivityLabel", "Connectivity: "), CachedBreakdown.ConnectivityScore, 0);
		AddAnimatedLine(Txt_TrafficOutcomeScore, LOCTEXT("TrafficOutcomeLabel", "Traffic Outcome: "), CachedBreakdown.TrafficOutcomeScore, 0);
		AddAnimatedLine(Txt_TravelEfficiencyScore, LOCTEXT("TravelEfficiencyLabel", "Travel Efficiency: "), CachedBreakdown.TravelEfficiencyScore, 0);
		AddAnimatedLine(Txt_BudgetEfficiencyScore, LOCTEXT("BudgetEfficiencyLabel", "Budget Efficiency: "), CachedBreakdown.BudgetEfficiencyScore, 0);
		AddAnimatedTimeLine(Txt_RuntimeScore, LOCTEXT("RuntimeLabel", "Runtime: "), CachedBreakdown.ElapsedSimulationTime);
		AddAnimatedLine(Txt_MapDifficultyMultiplier, LOCTEXT("DifficultyMultiplierLabel", "Map Difficulty Multiplier: "), CachedBreakdown.MapDifficultyMultiplier, 2, LOCTEXT("MultiplierPrefix", "x"));
	}
	else
	{
		AddAnimatedLine(Txt_Arrivals, LOCTEXT("LegacyArrivalsLabel", "Arrivals: "), CachedArrivals);
		AddAnimatedLine(Txt_Penalty, LOCTEXT("PenaltyLabel", "Penalty: -"), CachedPenalty);
	}

	AddAnimatedLine(Txt_HighScore, LOCTEXT("HighScoreLabel", "High Score: "), GlobalHighScore);
	AddAnimatedTimeLine(Txt_SimulationTime, LOCTEXT("SimulationTimeLabel", "Time: "), CachedElapsedTime);
}

void UCityFlowEvaluationWidget::EnsureScoreReportTextBlocks()
{
	if (!ScoreReportPanel || !WidgetTree)
	{
		return;
	}

	if (!Txt_StarRating)
	{
		CreateStarRatingImageRow();
	}
	EnsureStarRatingImages();
	if (!Txt_RawScore)
	{
		Txt_RawScore = CreateScoreReportTextBlock(TEXT("Txt_RawScore"));
	}
	if (!Txt_ConnectedBuildings)
	{
		Txt_ConnectedBuildings = CreateScoreReportTextBlock(TEXT("Txt_ConnectedBuildings"));
	}
	if (!Txt_LargestConnectedNetwork)
	{
		Txt_LargestConnectedNetwork = CreateScoreReportTextBlock(TEXT("Txt_LargestConnectedNetwork"));
	}
	if (!Txt_BudgetUsed)
	{
		Txt_BudgetUsed = CreateScoreReportTextBlock(TEXT("Txt_BudgetUsed"));
	}
	if (!Txt_EstimatedMinimumRoadNeed)
	{
		Txt_EstimatedMinimumRoadNeed = CreateScoreReportTextBlock(TEXT("Txt_EstimatedMinimumRoadNeed"));
	}
	if (!Txt_Deaths)
	{
		Txt_Deaths = CreateScoreReportTextBlock(TEXT("Txt_Deaths"));
	}
	if (!Txt_ArrivalRate)
	{
		Txt_ArrivalRate = CreateScoreReportTextBlock(TEXT("Txt_ArrivalRate"));
	}
	if (!Txt_AverageCellTravelTime)
	{
		Txt_AverageCellTravelTime = CreateScoreReportTextBlock(TEXT("Txt_AverageCellTravelTime"));
	}
	if (!Txt_ConnectivityScore)
	{
		Txt_ConnectivityScore = CreateScoreReportTextBlock(TEXT("Txt_ConnectivityScore"));
	}
	if (!Txt_TrafficOutcomeScore)
	{
		Txt_TrafficOutcomeScore = CreateScoreReportTextBlock(TEXT("Txt_TrafficOutcomeScore"));
	}
	if (!Txt_TravelEfficiencyScore)
	{
		Txt_TravelEfficiencyScore = CreateScoreReportTextBlock(TEXT("Txt_TravelEfficiencyScore"));
	}
	if (!Txt_BudgetEfficiencyScore)
	{
		Txt_BudgetEfficiencyScore = CreateScoreReportTextBlock(TEXT("Txt_BudgetEfficiencyScore"));
	}
	if (!Txt_RuntimeScore)
	{
		Txt_RuntimeScore = CreateScoreReportTextBlock(TEXT("Txt_RuntimeScore"));
	}
	if (!Txt_MapDifficultyMultiplier)
	{
		Txt_MapDifficultyMultiplier = CreateScoreReportTextBlock(TEXT("Txt_MapDifficultyMultiplier"));
	}
}

UTextBlock* UCityFlowEvaluationWidget::CreateScoreReportTextBlock(FName WidgetName)
{
	if (!ScoreReportPanel || !WidgetTree)
	{
		return nullptr;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("%s_Row"), *WidgetName.ToString())));
	UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%s_Label"), *WidgetName.ToString())));
	UTextBlock* ValueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
	if (!Row || !LabelTextBlock || !ValueTextBlock)
	{
		return nullptr;
	}

	ApplyScoreTextStyle(LabelTextBlock, ETextJustify::Left);
	ApplyScoreTextStyle(ValueTextBlock, ETextJustify::Right);

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelTextBlock))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueTextBlock))
	{
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = ScoreReportPanel->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.0f, 2.0f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	FScoreReportTextPair Pair;
	Pair.RowWidget = Row;
	Pair.LabelTextBlock = LabelTextBlock;
	Pair.ValueTextBlock = ValueTextBlock;
	GeneratedScoreRows.Add(ValueTextBlock, Pair);

	return ValueTextBlock;
}

void UCityFlowEvaluationWidget::EnsureStarRatingImages()
{
	if (!StarRatingPanel || !WidgetTree)
	{
		return;
	}

	while (StarImages.Num() < 3)
	{
		const int32 StarIndex = StarImages.Num();
		UImage* StarImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("Img_Star_%d"), StarIndex + 1)));
		if (!StarImage)
		{
			return;
		}

		if (FilledStarTexture)
		{
			StarImage->SetBrushFromTexture(FilledStarTexture, true);
		}

		StarImage->SetDesiredSizeOverride(StarImageSize);
		StarImage->SetRenderOpacity(EmptyStarOpacity);

		if (UHorizontalBoxSlot* StarSlot = StarRatingPanel->AddChildToHorizontalBox(StarImage))
		{
			StarSlot->SetPadding(FMargin(3.0f, 0.0f));
			StarSlot->SetHorizontalAlignment(HAlign_Right);
			StarSlot->SetVerticalAlignment(VAlign_Center);
		}

		StarImages.Add(StarImage);
	}

	UpdateStarRatingVisual(0);
}

void UCityFlowEvaluationWidget::CreateStarRatingImageRow()
{
	if (!ScoreReportPanel || !WidgetTree || StarRatingPanel)
	{
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StarRating_Row"));
	UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StarRating_Label"));
	UHorizontalBox* ImagePanel = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StarRatingPanel"));
	if (!Row || !LabelTextBlock || !ImagePanel)
	{
		return;
	}

	ApplyScoreTextStyle(LabelTextBlock, ETextJustify::Left);

	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelTextBlock))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UHorizontalBoxSlot* StarSlot = Row->AddChildToHorizontalBox(ImagePanel))
	{
		StarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		StarSlot->SetHorizontalAlignment(HAlign_Right);
		StarSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = ScoreReportPanel->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.0f, 2.0f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	StarRatingPanel = ImagePanel;
	GeneratedStarRatingRow = Row;
	GeneratedStarRatingLabel = LabelTextBlock;
}

void UCityFlowEvaluationWidget::ApplyScoreTextStyle(UTextBlock* TextBlock, ETextJustify::Type Justification) const
{
	if (!TextBlock)
	{
		return;
	}

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.OutlineSettings.OutlineSize = FMath::Max(Font.OutlineSettings.OutlineSize, 1);
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	TextBlock->SetFont(Font);
	TextBlock->SetJustification(Justification);
	TextBlock->SetAutoWrapText(false);
}

int32 UCityFlowEvaluationWidget::CalculateStarRating() const
{
	if (CachedTotalScore >= 800)
	{
		return 3;
	}
	if (CachedTotalScore >= 600)
	{
		return 2;
	}
	if (CachedTotalScore >= 350)
	{
		return 1;
	}
	return 0;
}

FText UCityFlowEvaluationWidget::MakeStarRatingText(int32 StarCount) const
{
	switch (FMath::Clamp(StarCount, 0, 3))
	{
	case 1: return LOCTEXT("OneStarRating", "★ ☆ ☆");
	case 2: return LOCTEXT("TwoStarRating", "★ ★ ☆");
	case 3: return LOCTEXT("ThreeStarRating", "★ ★ ★");
	default: return LOCTEXT("ZeroStarRating", "☆ ☆ ☆");
	}
}

void UCityFlowEvaluationWidget::UpdateStarRatingVisual(int32 StarCount) const
{
	const int32 ClampedStars = FMath::Clamp(StarCount, 0, 3);
	for (int32 i = 0; i < StarImages.Num(); ++i)
	{
		UImage* StarImage = StarImages[i];
		if (!StarImage)
		{
			continue;
		}

		const bool bFilled = i < ClampedStars;
		if (bFilled && FilledStarTexture)
		{
			StarImage->SetBrushFromTexture(FilledStarTexture, true);
		}
		else if (!bFilled && EmptyStarTexture)
		{
			StarImage->SetBrushFromTexture(EmptyStarTexture, true);
		}
		else if (FilledStarTexture)
		{
			StarImage->SetBrushFromTexture(FilledStarTexture, true);
		}

		StarImage->SetDesiredSizeOverride(StarImageSize);
		StarImage->SetRenderOpacity(bFilled ? 1.0f : EmptyStarOpacity);
	}
}

void UCityFlowEvaluationWidget::AddAnimatedLine(UTextBlock* TextBlock, const FText& Label, double TargetValue, int32 DecimalPlaces, const FText& Prefix, const FText& Suffix)
{
	if (!TextBlock)
	{
		return;
	}

	FAnimatedScoreLine Line;
	Line.TextBlock = TextBlock;
	Line.Label = Label;
	Line.TargetValue = TargetValue;
	Line.DecimalPlaces = DecimalPlaces;
	Line.Prefix = Prefix;
	Line.Suffix = Suffix;
	Line.bFormatAsStars = TextBlock == Txt_StarRating;

	if (const FScoreReportTextPair* GeneratedRow = GeneratedScoreRows.Find(TextBlock))
	{
		Line.RowWidget = GeneratedRow->RowWidget;
		Line.LabelTextBlock = GeneratedRow->LabelTextBlock;
	}
	else
	{
		ApplyScoreTextStyle(TextBlock, ETextJustify::Right);
	}

	AnimatedScoreLines.Add(Line);
}

void UCityFlowEvaluationWidget::AddAnimatedStarLine(const FText& Label, int32 TargetStarCount)
{
	EnsureStarRatingImages();

	if (!StarRatingPanel && !Txt_StarRating)
	{
		return;
	}

	FAnimatedScoreLine Line;
	Line.RowWidget = GeneratedStarRatingRow;
	Line.LabelTextBlock = GeneratedStarRatingLabel;
	Line.TextBlock = Txt_StarRating;
	Line.StarPanel = StarRatingPanel;
	Line.Label = Label;
	Line.TargetValue = TargetStarCount;
	Line.bFormatAsStars = true;
	Line.bUseStarImages = StarRatingPanel != nullptr && StarImages.Num() > 0;

	if (Txt_StarRating && !Line.bUseStarImages)
	{
		ApplyScoreTextStyle(Txt_StarRating, ETextJustify::Right);
	}

	AnimatedScoreLines.Add(Line);
}

void UCityFlowEvaluationWidget::AddAnimatedTimeLine(UTextBlock* TextBlock, const FText& Label, float TargetSeconds)
{
	if (!TextBlock)
	{
		return;
	}

	FAnimatedScoreLine Line;
	Line.TextBlock = TextBlock;
	Line.Label = Label;
	Line.TargetValue = TargetSeconds;
	Line.bFormatAsTime = true;

	if (const FScoreReportTextPair* GeneratedRow = GeneratedScoreRows.Find(TextBlock))
	{
		Line.RowWidget = GeneratedRow->RowWidget;
		Line.LabelTextBlock = GeneratedRow->LabelTextBlock;
	}
	else
	{
		ApplyScoreTextStyle(TextBlock, ETextJustify::Right);
	}

	AnimatedScoreLines.Add(Line);
}

void UCityFlowEvaluationWidget::StartScoreAnimation()
{
	if (AnimatedScoreLines.Num() == 0)
	{
		return;
	}

	if (!bAnimateScoreReport)
	{
		StopScoreAnimation(true);
		return;
	}

	SetScoreLineVisibility(ESlateVisibility::Collapsed);
	bScoreAnimationActive = true;
	CurrentAnimatedLineIndex = 0;
	CurrentLineElapsed = 0.0f;
	CurrentLineDelayRemaining = 0.0f;
	ShowCurrentAnimatedLine();
}

void UCityFlowEvaluationWidget::StopScoreAnimation(bool bComplete)
{
	bScoreAnimationActive = false;
	CurrentAnimatedLineIndex = INDEX_NONE;
	CurrentLineElapsed = 0.0f;
	CurrentLineDelayRemaining = 0.0f;

	if (bComplete)
	{
		for (const FAnimatedScoreLine& Line : AnimatedScoreLines)
		{
			if (UWidget* RowWidget = Line.RowWidget.Get())
			{
				RowWidget->SetVisibility(ESlateVisibility::Visible);
			}
			if (UTextBlock* LabelTextBlock = Line.LabelTextBlock.Get())
			{
				LabelTextBlock->SetVisibility(ESlateVisibility::Visible);
			}
			if (UHorizontalBox* StarPanel = Line.StarPanel.Get())
			{
				StarPanel->SetVisibility(ESlateVisibility::Visible);
			}
			if (UTextBlock* TextBlock = Line.TextBlock.Get())
			{
				TextBlock->SetVisibility(ESlateVisibility::Visible);
			}
			SetAnimatedLineValue(Line, Line.TargetValue);
		}
	}
}

void UCityFlowEvaluationWidget::AdvanceScoreAnimation(float DeltaTime)
{
	if (!AnimatedScoreLines.IsValidIndex(CurrentAnimatedLineIndex))
	{
		StopScoreAnimation(true);
		return;
	}

	if (CurrentLineDelayRemaining > 0.0f)
	{
		CurrentLineDelayRemaining -= DeltaTime;
		if (CurrentLineDelayRemaining > 0.0f)
		{
			return;
		}

		ShowCurrentAnimatedLine();
	}

	const FAnimatedScoreLine& Line = AnimatedScoreLines[CurrentAnimatedLineIndex];
	CurrentLineElapsed += DeltaTime;

	const float Duration = FMath::Max(NumberRollDuration, 0.01f);
	const float Alpha = FMath::Clamp(CurrentLineElapsed / Duration, 0.0f, 1.0f);
	const float EaseOut = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
	SetAnimatedLineValue(Line, Line.TargetValue * EaseOut);

	if (Alpha >= 1.0f)
	{
		SetAnimatedLineValue(Line, Line.TargetValue);
		++CurrentAnimatedLineIndex;
		CurrentLineElapsed = 0.0f;

		if (!AnimatedScoreLines.IsValidIndex(CurrentAnimatedLineIndex))
		{
			StopScoreAnimation(true);
			return;
		}

		CurrentLineDelayRemaining = LineRevealDelay;
	}
}

void UCityFlowEvaluationWidget::ShowCurrentAnimatedLine()
{
	if (!AnimatedScoreLines.IsValidIndex(CurrentAnimatedLineIndex))
	{
		StopScoreAnimation(true);
		return;
	}

	const FAnimatedScoreLine& Line = AnimatedScoreLines[CurrentAnimatedLineIndex];
	if (UWidget* RowWidget = Line.RowWidget.Get())
	{
		RowWidget->SetVisibility(ESlateVisibility::Visible);
	}
	if (UTextBlock* LabelTextBlock = Line.LabelTextBlock.Get())
	{
		LabelTextBlock->SetVisibility(ESlateVisibility::Visible);
		LabelTextBlock->SetText(Line.Label);
	}
	if (UHorizontalBox* StarPanel = Line.StarPanel.Get())
	{
		StarPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (UTextBlock* TextBlock = Line.TextBlock.Get())
	{
		TextBlock->SetVisibility(ESlateVisibility::Visible);
	}
	SetAnimatedLineValue(Line, 0.0);
}

void UCityFlowEvaluationWidget::SetAnimatedLineValue(const FAnimatedScoreLine& Line, double Value) const
{
	if (UTextBlock* LabelTextBlock = Line.LabelTextBlock.Get())
	{
		LabelTextBlock->SetText(Line.Label);
	}

	if (Line.bUseStarImages)
	{
		UpdateStarRatingVisual(FMath::RoundToInt(Value));
		return;
	}

	if (UTextBlock* TextBlock = Line.TextBlock.Get())
	{
		TextBlock->SetText(FormatAnimatedValue(Line, Value));
	}
}

FText UCityFlowEvaluationWidget::FormatAnimatedValue(const FAnimatedScoreLine& Line, double Value) const
{
	const auto CombineLabelAndValue = [&Line](const FText& ValueText)
	{
		if (Line.LabelTextBlock.IsValid())
		{
			return ValueText;
		}
		FFormatNamedArguments Args;
		Args.Add(TEXT("Label"), Line.Label);
		Args.Add(TEXT("Value"), ValueText);
		return FText::Format(LOCTEXT("LabelValueFormat", "{Label}{Value}"), Args);
	};

	if (Line.bFormatAsTime)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::RoundToInt(Value));
		const int32 Mins = TotalSeconds / 60;
		const int32 Secs = TotalSeconds % 60;
		FNumberFormattingOptions TwoDigitOptions;
		TwoDigitOptions.MinimumIntegralDigits = 2;
		TwoDigitOptions.MaximumIntegralDigits = 2;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, &TwoDigitOptions));
		Args.Add(TEXT("Seconds"), FText::AsNumber(Secs, &TwoDigitOptions));
		return CombineLabelAndValue(FText::Format(LOCTEXT("TimeValueFormat", "{Minutes}:{Seconds}"), Args));
	}

	if (Line.bFormatAsStars)
	{
		return CombineLabelAndValue(MakeStarRatingText(FMath::RoundToInt(Value)));
	}

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = FMath::Max(0, Line.DecimalPlaces);
	NumberOptions.MaximumFractionalDigits = FMath::Max(0, Line.DecimalPlaces);
	const FText NumberText = Line.DecimalPlaces <= 0
		? FText::AsNumber(FMath::RoundToInt(Value))
		: FText::AsNumber(Value, &NumberOptions);
	FFormatNamedArguments Args;
	Args.Add(TEXT("Prefix"), Line.Prefix);
	Args.Add(TEXT("Number"), NumberText);
	Args.Add(TEXT("Suffix"), Line.Suffix);
	return CombineLabelAndValue(FText::Format(LOCTEXT("DecoratedNumberFormat", "{Prefix}{Number}{Suffix}"), Args));
}

void UCityFlowEvaluationWidget::SetScoreLineVisibility(ESlateVisibility InVisibility)
{
	for (const FAnimatedScoreLine& Line : AnimatedScoreLines)
	{
		if (UWidget* RowWidget = Line.RowWidget.Get())
		{
			RowWidget->SetVisibility(InVisibility);
		}
		if (UTextBlock* LabelTextBlock = Line.LabelTextBlock.Get())
		{
			LabelTextBlock->SetVisibility(InVisibility);
		}
		if (UHorizontalBox* StarPanel = Line.StarPanel.Get())
		{
			StarPanel->SetVisibility(InVisibility);
		}
		if (UTextBlock* TextBlock = Line.TextBlock.Get())
		{
			TextBlock->SetVisibility(InVisibility);
		}
	}
}

void UCityFlowEvaluationWidget::HandleBackToMainClicked()
{
	OnBackToMainClicked.Broadcast();
}

void UCityFlowEvaluationWidget::HandleRestartClicked()
{
	OnRestartClicked.Broadcast();
}

#undef LOCTEXT_NAMESPACE
