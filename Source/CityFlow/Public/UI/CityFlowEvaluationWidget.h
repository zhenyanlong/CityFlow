#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Scoring/Types/ScoringTypes.h"
#include "CityFlowEvaluationWidget.generated.h"

/** Presents the final simulation report and offers restart or main-menu actions. */
UCLASS()
class CITYFLOW_API UCityFlowEvaluationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackToMainClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRestartClicked);

	/** Broadcast when the player selects Back to Main Menu. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnBackToMainClicked OnBackToMainClicked;

	/** Broadcast when the player selects Restart. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnRestartClicked OnRestartClicked;

	/** Supplies the complete immutable result snapshot and refreshes every field. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void Populate(int32 TotalScore, int32 Arrivals, int32 Penalty, float ElapsedTime);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void PopulateFromBreakdown(const FCityFlowScoreBreakdown& Breakdown);

protected:
	// ---- Blueprint BindWidget controls ----

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_BackToMain;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Restart;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TotalScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Arrivals;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Penalty;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_HighScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SimulationTime;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ScoreReportPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ConnectedBuildings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_LargestConnectedNetwork;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BudgetUsed;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EstimatedMinimumRoadNeed;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Deaths;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ArrivalRate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_AverageCellTravelTime;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ConnectivityScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TrafficOutcomeScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TravelEfficiencyScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BudgetEfficiencyScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_RuntimeScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_MapDifficultyMultiplier;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_RawScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StarRating;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> StarRatingPanel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Stars")
	TObjectPtr<UTexture2D> FilledStarTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Stars")
	TObjectPtr<UTexture2D> EmptyStarTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Stars", meta = (ClampMin = "8.0", ClampMax = "256.0"))
	FVector2D StarImageSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Stars", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmptyStarOpacity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Animation", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float NumberRollDuration = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Animation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LineRevealDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|UI|Animation")
	bool bAnimateScoreReport = true;

private:
	struct FAnimatedScoreLine
	{
		TWeakObjectPtr<UWidget> RowWidget;
		TWeakObjectPtr<UTextBlock> LabelTextBlock;
		TWeakObjectPtr<UTextBlock> TextBlock;
		TWeakObjectPtr<UHorizontalBox> StarPanel;
		FText Label;
		double TargetValue = 0.0;
		int32 DecimalPlaces = 0;
		FText Prefix;
		FText Suffix;
		bool bFormatAsTime = false;
		bool bFormatAsStars = false;
		bool bUseStarImages = false;
	};

	struct FScoreReportTextPair
	{
		TWeakObjectPtr<UWidget> RowWidget;
		TWeakObjectPtr<UTextBlock> LabelTextBlock;
		TWeakObjectPtr<UTextBlock> ValueTextBlock;
	};

	UFUNCTION()
	void HandleBackToMainClicked();

	UFUNCTION()
	void HandleRestartClicked();

	void RefreshUI();
	void EnsureScoreReportTextBlocks();
	UTextBlock* CreateScoreReportTextBlock(FName WidgetName);
	void EnsureStarRatingImages();
	void CreateStarRatingImageRow();
	void ApplyScoreTextStyle(UTextBlock* TextBlock, ETextJustify::Type Justification) const;
	int32 CalculateStarRating() const;
	FText MakeStarRatingText(int32 StarCount) const;
	void UpdateStarRatingVisual(int32 StarCount) const;
	void BuildAnimatedScoreLines();
	void AddAnimatedLine(UTextBlock* TextBlock, const FText& Label, double TargetValue, int32 DecimalPlaces = 0, const FText& Prefix = FText::GetEmpty(), const FText& Suffix = FText::GetEmpty());
	void AddAnimatedStarLine(const FText& Label, int32 TargetStarCount);
	void AddAnimatedTimeLine(UTextBlock* TextBlock, const FText& Label, float TargetSeconds);
	void StartScoreAnimation();
	void StopScoreAnimation(bool bComplete);
	void AdvanceScoreAnimation(float DeltaTime);
	void ShowCurrentAnimatedLine();
	void SetAnimatedLineValue(const FAnimatedScoreLine& Line, double Value) const;
	FText FormatAnimatedValue(const FAnimatedScoreLine& Line, double Value) const;
	void SetScoreLineVisibility(ESlateVisibility InVisibility);

	int32 CachedTotalScore = 0;
	int32 CachedArrivals = 0;
	int32 CachedPenalty = 0;
	float CachedElapsedTime = 0.0f;
	FCityFlowScoreBreakdown CachedBreakdown;
	bool bHasScoreBreakdown = false;

	/** Session-only high score; intentionally not persisted between processes. */
	bool bScoreAnimationActive = false;
	int32 CurrentAnimatedLineIndex = INDEX_NONE;
	float CurrentLineElapsed = 0.0f;
	float CurrentLineDelayRemaining = 0.0f;
	TArray<FAnimatedScoreLine> AnimatedScoreLines;
	TMap<TWeakObjectPtr<UTextBlock>, FScoreReportTextPair> GeneratedScoreRows;
	TWeakObjectPtr<UWidget> GeneratedStarRatingRow;
	TWeakObjectPtr<UTextBlock> GeneratedStarRatingLabel;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> StarImages;

	// Runtime high score for the current process.
	static int32 GlobalHighScore;
};
