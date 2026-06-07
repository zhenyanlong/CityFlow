#include "UI/CityFlowEvaluationWidget.h"

int32 UCityFlowEvaluationWidget::GlobalHighScore = 0;

void UCityFlowEvaluationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_BackToMain)
		Btn_BackToMain->OnClicked.AddDynamic(this, &UCityFlowEvaluationWidget::HandleBackToMainClicked);
	if (Btn_Restart)
		Btn_Restart->OnClicked.AddDynamic(this, &UCityFlowEvaluationWidget::HandleRestartClicked);
}

void UCityFlowEvaluationWidget::NativeDestruct()
{
	if (Btn_BackToMain)
		Btn_BackToMain->OnClicked.RemoveAll(this);
	if (Btn_Restart)
		Btn_Restart->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowEvaluationWidget::Populate(int32 TotalScore, int32 Arrivals, int32 Penalty, float ElapsedTime)
{
	CachedTotalScore = TotalScore;
	CachedArrivals = Arrivals;
	CachedPenalty = Penalty;
	CachedElapsedTime = ElapsedTime;

	if (TotalScore > GlobalHighScore)
		GlobalHighScore = TotalScore;

	RefreshUI();
}

void UCityFlowEvaluationWidget::RefreshUI()
{
	if (Txt_TotalScore)
		Txt_TotalScore->SetText(FText::FromString(FString::Printf(TEXT("Score: %d"), CachedTotalScore)));

	if (Txt_Arrivals)
		Txt_Arrivals->SetText(FText::FromString(FString::Printf(TEXT("Arrivals: %d"), CachedArrivals)));

	if (Txt_Penalty)
		Txt_Penalty->SetText(FText::FromString(FString::Printf(TEXT("Penalty: -%d"), CachedPenalty)));

	if (Txt_HighScore)
		Txt_HighScore->SetText(FText::FromString(FString::Printf(TEXT("High Score: %d"), GlobalHighScore)));

	if (Txt_SimulationTime)
	{
		const int32 Mins = FMath::FloorToInt(CachedElapsedTime / 60.0f);
		const int32 Secs = FMath::FloorToInt(FMath::Fmod(CachedElapsedTime, 60.0f));
		Txt_SimulationTime->SetText(FText::FromString(FString::Printf(TEXT("Time: %02d:%02d"), Mins, Secs)));
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
