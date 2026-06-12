#include "UI/CityFlowGameWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Grid/GridManager.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "UI/ScorePopupWidget.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Player/CityFlowPlayerController.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	{
		SM->OnScoreChanged.AddDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
		SM->OnScorePopupRequested.AddDynamic(this, &UCityFlowGameWidget::HandleScorePopupRequested);
	}

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	// 监听网格变化，随时刷新预算显示
	if (UGridManager* GridMgr = GetWorld()->GetSubsystem<UGridManager>())
		GridMgr->OnCellChanged.AddDynamic(this, &UCityFlowGameWidget::HandleCellChanged);

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
	{
		SM->OnScoreChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
		SM->OnScorePopupRequested.RemoveDynamic(this, &UCityFlowGameWidget::HandleScorePopupRequested);
	}

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	if (UGridManager* GridMgr = GetWorld()->GetSubsystem<UGridManager>())
		GridMgr->OnCellChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleCellChanged);

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

	if (NewPhase == ECityFlowGamePhase::Simulating)
		StartCountdown();
	else
		StopCountdown();

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

void UCityFlowGameWidget::HandleScorePopupRequested(FVector WorldLocation, int32 DeltaScore)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	TSubclassOf<UScorePopupWidget> PopupClass = ScorePopupWidgetClass;
	if (!PopupClass)
	{
		PopupClass = UScorePopupWidget::StaticClass();
	}

	UScorePopupWidget* Popup = CreateWidget<UScorePopupWidget>(PC, PopupClass);
	if (!Popup)
	{
		return;
	}

	if (PopupLayer)
	{
		UCanvasPanelSlot* CanvasSlot = PopupLayer->AddChildToCanvas(Popup);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
	}
	else
	{
		Popup->AddToViewport(20);
	}

	const FLinearColor PopupColor = DeltaScore >= 0 ? PositivePopupColor : NegativePopupColor;
	Popup->InitializeScreenPopup(PC, WorldLocation, DeltaScore, PopupColor);
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

	const int32 Remaining = GetRemainingBudget();
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
//  网格变更 → 刷新预算
// ============================================================================

void UCityFlowGameWidget::HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell)
{
	UpdateBudgetText();
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

int32 UCityFlowGameWidget::GetRemainingBudget() const
{
	if (UGridManager* GM = GetWorld()->GetSubsystem<UGridManager>())
		return GM->GetRemainingBudget();
	return 0;
}

// ============================================================================
//  倒计时
// ============================================================================

void UCityFlowGameWidget::StartCountdown()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (!GM) return;

	CountdownSeconds = FMath::CeilToInt(GM->SimulationDuration);
	UpdateCountdownText();

	GetWorld()->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&UCityFlowGameWidget::TickCountdown,
		1.0f,
		true
	);
}

void UCityFlowGameWidget::TickCountdown()
{
	--CountdownSeconds;
	UpdateCountdownText();

	if (CountdownSeconds <= 0)
	{
		StopCountdown();
	}
}

void UCityFlowGameWidget::StopCountdown()
{
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
}

void UCityFlowGameWidget::UpdateCountdownText()
{
	if (!Txt_Countdown) return;

	const int32 Mins = CountdownSeconds / 60;
	const int32 Secs = CountdownSeconds % 60;
	Txt_Countdown->SetText(FText::FromString(
		FString::Printf(TEXT("%02d:%02d"), Mins, Secs)));
}
