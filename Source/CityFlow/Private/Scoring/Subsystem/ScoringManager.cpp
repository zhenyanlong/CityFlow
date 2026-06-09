#include "Scoring/Subsystem/ScoringManager.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Grid/GridManager.h"
#include "Grid/Building.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogScoring, Log, All);

void UScoringManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UScoringManager::Deinitialize()
{
	StopScoring();
	Super::Deinitialize();
}

void UScoringManager::StartScoring()
{
	StopScoring();

	TotalScore = 0;
	ArrivalScoreTotal = 0;
	CongestionPenaltyTotal = 0;
	DeathPenaltyTotal = 0;
	DeathCount = 0;
	TotalArrivalCount = 0;
	ElapsedSimulationTime = 0.0f;
	CurrentCongestedCells.Empty();
	FinalCongestionCellCount = 0;

	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->OnVehicleArrived.AddDynamic(this, &UScoringManager::OnVehicleArrivedHandler);
		VM->OnCongestionUpdated.AddDynamic(this, &UScoringManager::OnCongestionUpdatedHandler);
		VM->OnVehicleDied.AddDynamic(this, &UScoringManager::OnVehicleDeathHandler);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			PenaltyTimerHandle,
			this,
			&UScoringManager::UpdateCongestionPenalty,
			PENALTY_CHECK_INTERVAL,
			true,
			0.0f
		);
	}

	bIsScoring = true;
}

void UScoringManager::StopScoring()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PenaltyTimerHandle);
	}

	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->OnVehicleArrived.RemoveDynamic(this, &UScoringManager::OnVehicleArrivedHandler);
		VM->OnCongestionUpdated.RemoveDynamic(this, &UScoringManager::OnCongestionUpdatedHandler);
		VM->OnVehicleDied.RemoveDynamic(this, &UScoringManager::OnVehicleDeathHandler);
	}

	bIsScoring = false;
	OnSimulationEvaluation.Broadcast();
}

FString UScoringManager::GetPhaseSummary() const
{
	return FString::Printf(TEXT("Score: %d | Arrivals: %d (+%d) | Congestion: -%d | Deaths: %d (-%d)"),
		TotalScore, TotalArrivalCount, ArrivalScoreTotal, CongestionPenaltyTotal, DeathCount, DeathPenaltyTotal);
}

void UScoringManager::RecordVehicleArrival(AVehicleActor* Vehicle)
{
	if (!Vehicle || !bIsScoring)
	{
		return;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const int32 ArrivalPoints = Settings ? Settings->ArrivalScore : 100;

	++TotalArrivalCount;
	ArrivalScoreTotal += ArrivalPoints;
	TotalScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal;

	OnScoreChanged.Broadcast(TotalScore);
}

void UScoringManager::RecordCongestion(const TSet<FGridVector>& CongestedCells)
{
	CurrentCongestedCells = CongestedCells;
}

void UScoringManager::OnVehicleArrivedHandler(AVehicleActor* Vehicle)
{
	RecordVehicleArrival(Vehicle);
}

void UScoringManager::OnVehicleDeathHandler(AVehicleActor* Vehicle)
{
	if (!Vehicle || !bIsScoring)
	{
		return;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const int32 Penalty = Settings ? Settings->DeathPenalty : 50;

	++DeathCount;
	DeathPenaltyTotal += Penalty;
	TotalScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal;

	OnScoreChanged.Broadcast(TotalScore);
}

void UScoringManager::OnCongestionUpdatedHandler()
{
	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		CurrentCongestedCells = VM->GetCongestedCells();
	}
}

void UScoringManager::UpdateCongestionPenalty()
{
	if (!bIsScoring)
	{
		return;
	}

	ElapsedSimulationTime += PENALTY_CHECK_INTERVAL;

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (!Settings)
	{
		return;
	}

	const int32 CongestedCount = CurrentCongestedCells.Num();
	if (CongestedCount > 0)
	{
		const int32 Penalty = CongestedCount * Settings->CongestionPenaltyPerSecond;
		CongestionPenaltyTotal += Penalty;
		FinalCongestionCellCount = FMath::Max(FinalCongestionCellCount, CongestedCount);
	}

	TotalScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal;
	OnScoreChanged.Broadcast(TotalScore);
}

void UScoringManager::ComputeFinalScore(bool bAllConnected)
{
	if (bAllConnected)
	{
		const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
		if (Settings)
		{
			TotalScore += Settings->FullConnectivityBonus;
		}
	}

	OnScoreChanged.Broadcast(TotalScore);
}

UVehicleManager* UScoringManager::GetVehicleManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UVehicleManager>();
}

UGridManager* UScoringManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}
