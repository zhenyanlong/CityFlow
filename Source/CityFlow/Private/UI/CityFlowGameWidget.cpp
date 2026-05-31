#include "UI/CityFlowGameWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCityFlowGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
}

void UCityFlowGameWidget::NativeDestruct()
{
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

void UCityFlowGameWidget::StartSimulation()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->StartSimulationPhase();
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
}

void UCityFlowGameWidget::TriggerLSystem()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		GM->TriggerLSystemGrowth();
	}
}

void UCityFlowGameWidget::HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase)
{
	OnPhaseChanged_BP(OldPhase, NewPhase);

	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (GM)
	{
		OnBudgetChanged_BP(GM->GetPlayerBudget(), GM->GetLSystemBudget(), GM->GetRemainingBudget());
	}
}

void UCityFlowGameWidget::HandleScoreChanged(int32 NewScore)
{
	OnScoreChanged_BP(NewScore);
}

void UCityFlowGameWidget::HandleLSystemStep(int32 RemainingBudget)
{
	OnLSystemStep_BP(RemainingBudget);
}

void UCityFlowGameWidget::HandleLSystemFinished(bool bAllConnected)
{
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

ECityFlowGamePhase UCityFlowGameWidget::GetCurrentPhase() const
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	return GM ? GM->GetCurrentPhase() : ECityFlowGamePhase::None;
}

int32 UCityFlowGameWidget::GetTotalScore() const
{
	UScoringManager* SM = GetScoringManager();
	return SM ? SM->GetTotalScore() : 0;
}

int32 UCityFlowGameWidget::GetRemainingBudget() const
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	return GM ? GM->GetRemainingBudget() : 0;
}

float UCityFlowGameWidget::GetSimulationTimeRemaining() const
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	return GM ? GM->GetSimulationTimeRemaining() : 0.0f;
}

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
