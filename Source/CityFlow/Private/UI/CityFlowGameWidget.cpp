#include "UI/CityFlowGameWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Player/CityFlowPlayerController.h"
#include "Engine/World.h"

// ============================================================================
//  生命周期
// ============================================================================

void UCityFlowGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定按钮（回调必须是 UFUNCTION，否则 AddDynamic 静默失败）
	if (Btn_TriggerLSystem)
		Btn_TriggerLSystem->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnTriggerLSystemClicked);
	if (Btn_StartSimulation)
		Btn_StartSimulation->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnStartSimulationClicked);
	if (Btn_RestartPlanning)
		Btn_RestartPlanning->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnRestartPlanningClicked);

	// 绑定 GameMode / Scoring / LSystem 委托
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->OnGamePhaseChanged.AddDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);

	if (UScoringManager* SM = GetScoringManager())
		SM->OnScoreChanged.AddDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	// 初始 UI 状态
	UpdateButtonStates(GetCityFlowGameMode() ? GetCityFlowGameMode()->GetCurrentPhase() : ECityFlowGamePhase::None);
}

void UCityFlowGameWidget::NativeDestruct()
{
	// 解绑按钮
	if (Btn_TriggerLSystem)     Btn_TriggerLSystem->OnClicked.RemoveAll(this);
	if (Btn_StartSimulation)    Btn_StartSimulation->OnClicked.RemoveAll(this);
	if (Btn_RestartPlanning)    Btn_RestartPlanning->OnClicked.RemoveAll(this);

	// 解绑委托
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->OnGamePhaseChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);

	if (UScoringManager* SM = GetScoringManager())
		SM->OnScoreChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	Super::NativeDestruct();
}

// ============================================================================
//  按钮回调
// ============================================================================

void UCityFlowGameWidget::OnStartSimulationClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->StartSimulationPhase();

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
		PC->DisablePlacement();
}

void UCityFlowGameWidget::OnRestartPlanningClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->RestartPlanningPhase();

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
		PC->EnablePlacement();
}

void UCityFlowGameWidget::OnTriggerLSystemClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->TriggerLSystemGrowth();
}

// ============================================================================
//  委托回调
// ============================================================================

void UCityFlowGameWidget::HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase)
{
	UpdatePhaseText(NewPhase);
	UpdateBudgetText();
	UpdateButtonStates(NewPhase);

	OnPhaseChanged_BP(OldPhase, NewPhase);

	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		OnBudgetChanged_BP(GM->GetPlayerBudget(), GM->GetLSystemBudget(), GM->GetRemainingBudget());
}

void UCityFlowGameWidget::HandleScoreChanged(int32 NewScore)
{
	if (Txt_Score)
		Txt_Score->SetText(FText::FromString(FString::Printf(TEXT("Score: %d"), NewScore)));

	OnScoreChanged_BP(NewScore);
}

void UCityFlowGameWidget::HandleLSystemStep(int32 RemainingBudget)
{
	if (Txt_Budget)
		Txt_Budget->SetText(FText::FromString(FString::Printf(TEXT("Budget: %d"), RemainingBudget)));

	OnLSystemStep_BP(RemainingBudget);
}

void UCityFlowGameWidget::HandleLSystemFinished(bool bAllConnected)
{
	UpdateBudgetText();
	OnLSystemFinished_BP(bAllConnected);
}

// ============================================================================
//  UI 更新辅助
// ============================================================================

void UCityFlowGameWidget::UpdatePhaseText(ECityFlowGamePhase Phase)
{
	if (!Txt_Phase) return;

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
	if (!Txt_Budget) return;

	const int32 Remaining = GetCityFlowGameMode() ? GetCityFlowGameMode()->GetRemainingBudget() : 0;
	Txt_Budget->SetText(FText::FromString(FString::Printf(TEXT("Budget: %d"), Remaining)));
}

void UCityFlowGameWidget::UpdateButtonStates(ECityFlowGamePhase Phase)
{
	using EVis = ESlateVisibility;
	const bool bPlanning = (Phase == ECityFlowGamePhase::Planning);
	const bool bSimulating = (Phase == ECityFlowGamePhase::Simulating);

	if (Btn_TriggerLSystem)   Btn_TriggerLSystem->SetVisibility(bPlanning ? EVis::Visible : EVis::Collapsed);
	if (Btn_StartSimulation)  Btn_StartSimulation->SetVisibility(bPlanning ? EVis::Visible : EVis::Collapsed);
	if (Btn_RestartPlanning)  Btn_RestartPlanning->SetVisibility(bSimulating ? EVis::Visible : EVis::Collapsed);
}

// ============================================================================
//  辅助
// ============================================================================

ACityFlowGameMode* UCityFlowGameWidget::GetCityFlowGameMode() const
{
	return Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());
}

UScoringManager* UCityFlowGameWidget::GetScoringManager() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UScoringManager>() : nullptr;
}
