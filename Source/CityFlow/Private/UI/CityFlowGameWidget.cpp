#include "UI/CityFlowGameWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Player/CityFlowPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCityFlowGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定按钮点击事件
	if (Btn_TriggerLSystem)
	{
		Btn_TriggerLSystem->OnClicked.AddDynamic(this, &UCityFlowGameWidget::TriggerLSystem);
	}
	if (Btn_StartSimulation)
	{
		Btn_StartSimulation->OnClicked.AddDynamic(this, &UCityFlowGameWidget::StartSimulation);
	}
	if (Btn_RestartPlanning)
	{
		Btn_RestartPlanning->OnClicked.AddDynamic(this, &UCityFlowGameWidget::RestartPlanning);
	}

	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->OnGamePhaseChanged.AddDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);
	}

	UScoringManager* SM = GetScoringManager();
	if (SM)
	{
		SM->OnScoreChanged.AddDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (LSM)
	{
		LSM->OnGenerationStep.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	// 初始状态刷新
	UpdateButtonStates(GM ? GM->GetCurrentPhase() : ECityFlowGamePhase::None);
}

void UCityFlowGameWidget::NativeDestruct()
{
	if (Btn_TriggerLSystem)
	{
		Btn_TriggerLSystem->OnClicked.RemoveAll(this);
	}
	if (Btn_StartSimulation)
	{
		Btn_StartSimulation->OnClicked.RemoveAll(this);
	}
	if (Btn_RestartPlanning)
	{
		Btn_RestartPlanning->OnClicked.RemoveAll(this);
	}

	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->OnGamePhaseChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);
	}

	UScoringManager* SM = GetScoringManager();
	if (SM)
	{
		SM->OnScoreChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (LSM)
	{
		LSM->OnGenerationStep.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	Super::NativeDestruct();
}

// ---- 按钮逻辑 ----

void UCityFlowGameWidget::StartSimulation()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->StartSimulationPhase();
	}

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
	{
		PC->DisablePlacement();
	}
}

void UCityFlowGameWidget::EndSimulation()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->EndSimulationPhase();
	}
}

void UCityFlowGameWidget::RestartPlanning()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->RestartPlanningPhase();
	}

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
	{
		PC->EnablePlacement();
	}
}

void UCityFlowGameWidget::TriggerLSystem()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->TriggerLSystemGrowth();
	}
}

// ---- 委托回调 ----

void UCityFlowGameWidget::HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase)
{
	UpdatePhaseText(NewPhase);
	UpdateBudgetText();
	UpdateButtonStates(NewPhase);

	OnPhaseChanged_BP(OldPhase, NewPhase);

	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		OnBudgetChanged_BP(GM->GetPlayerBudget(), GM->GetLSystemBudget(), GM->GetRemainingBudget());
	}
}

void UCityFlowGameWidget::HandleScoreChanged(int32 NewScore)
{
	if (Txt_Score)
	{
		Txt_Score->SetText(FText::FromString(FString::Printf(TEXT("Score: %d"), NewScore)));
	}

	OnScoreChanged_BP(NewScore);
}

void UCityFlowGameWidget::HandleLSystemStep(int32 RemainingBudget)
{
	if (Txt_Budget)
	{
		Txt_Budget->SetText(FText::FromString(FString::Printf(TEXT("Budget: %d"), RemainingBudget)));
	}

	OnLSystemStep_BP(RemainingBudget);
}

void UCityFlowGameWidget::HandleLSystemFinished(bool bAllConnected)
{
	UpdateBudgetText();
	OnLSystemFinished_BP(bAllConnected);
}

void UCityFlowGameWidget::HandleSimulationEnd()
{
	UScoringManager* SM = GetScoringManager();
	if (!SM)
	{
		return;
	}

	OnEvaluation_BP(SM->GetTotalScore(), SM->GetArrivalCount(), SM->GetCongestionPenalty(), false);
}

// ---- UI 更新辅助 ----

void UCityFlowGameWidget::UpdatePhaseText(ECityFlowGamePhase Phase)
{
	if (!Txt_Phase)
	{
		return;
	}

	const TCHAR* PhaseStr = TEXT("Unknown");
	switch (Phase)
	{
	case ECityFlowGamePhase::Planning:   PhaseStr = TEXT("Planning");   break;
	case ECityFlowGamePhase::Simulating: PhaseStr = TEXT("Simulating"); break;
	case ECityFlowGamePhase::Evaluation: PhaseStr = TEXT("Evaluation"); break;
	}
	Txt_Phase->SetText(FText::FromString(FString::Printf(TEXT("Phase: %s"), PhaseStr)));
}

void UCityFlowGameWidget::UpdateBudgetText()
{
	if (!Txt_Budget)
	{
		return;
	}

	ACityFlowGameMode* GM = GetCityFlowGameMode();
	const int32 Remaining = GM ? GM->GetRemainingBudget() : 0;
	Txt_Budget->SetText(FText::FromString(FString::Printf(TEXT("Budget: %d"), Remaining)));
}

void UCityFlowGameWidget::UpdateButtonStates(ECityFlowGamePhase Phase)
{
	using EVis = ESlateVisibility;

	if (Btn_TriggerLSystem)
	{
		Btn_TriggerLSystem->SetVisibility(Phase == ECityFlowGamePhase::Planning ? EVis::Visible : EVis::Collapsed);
	}
	if (Btn_StartSimulation)
	{
		Btn_StartSimulation->SetVisibility(Phase == ECityFlowGamePhase::Planning ? EVis::Visible : EVis::Collapsed);
	}
	if (Btn_RestartPlanning)
	{
		Btn_RestartPlanning->SetVisibility(Phase == ECityFlowGamePhase::Evaluation ? EVis::Visible : EVis::Collapsed);
	}
}

// ---- 辅助 ----

ACityFlowGameMode* UCityFlowGameWidget::GetCityFlowGameMode() const
{
	return Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());
}

UScoringManager* UCityFlowGameWidget::GetScoringManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UScoringManager>();
}
